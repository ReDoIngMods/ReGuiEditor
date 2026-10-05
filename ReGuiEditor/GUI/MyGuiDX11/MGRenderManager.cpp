#include "MGRenderManager.hpp"

#include "MGTexture.hpp"
#include "MGVertexBuffer.hpp"

#include <UTILS/Console.hpp>

#include <rend/Common/CullingMode.hpp>
#include <rend/Common/Format.hpp>
#include <rend/Common/PrimitiveTopology.hpp>
#include <rend/Common/SamplerAddressMode.hpp>
#include <rend/Common/Viewport.hpp>

#include <rend/ResourceManager.hpp>

#include <rend/Resources/PipelineContext.hpp>
#include <rend/Resources/RenderView.hpp>

#include <rend/Resources/DepthStencilState.hpp>
#include <rend/Resources/RasterizerState.hpp>

#include <rend/Resources/Shaders/InputLayout.hpp>
#include <rend/Resources/Shaders/PixelShader.hpp>
#include <rend/Resources/Shaders/VertexShader.hpp>

#include <MyGui/MyGUI_Gui.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_IVertexBuffer.h>
#include <MyGui/MyGUI_RenderFormat.h>
#include <MyGui/MyGUI_Timer.h>

#include <algorithm>
#include <cstdint>
#include <string>

struct alignas(16) ConstantBuffer {
	float uvScale[2] = { 1.0f, 1.0f };
};

bool MyGUICustom::DirectX11RenderManager::initialise() {
	this->vertexFormat = MyGUI::VertexColourType::ColourABGR;

	this->setViewSize(1920, 1080);

	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();
	this->samplerState_ = resourceManager.createLinearSamplerState(rend::SamplerAddressMode::Wrap);
	if (!this->samplerState_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create sampler state.");
		return false;
	}
	this->samplerState_->setDebugName("MyGUI::SamplerState");

	this->blendState_ = resourceManager.createDefaultBlendState();
	if (!this->blendState_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create default blend state.");
		return false;
	}
	this->blendState_->setDebugName("MyGUI::BlendState");

	// MyGUI quads are counter-clockwise, so back-face culling would discard them.
	rend::RasterizerDesc rasterizerDesc;
	rasterizerDesc.cullMode = rend::CullingMode::None;
	rasterizerDesc.depthClip = false;

	this->rasterizerState_ = resourceManager.createRasterizerState(rasterizerDesc);
	if (!this->rasterizerState_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create rasterizer state.");
		return false;
	}
	this->rasterizerState_->setDebugName("MyGUI::RasterizerState");

	// MyGUI draws everything at the same depth and relies on draw order, so depth testing must be off.
	rend::DepthStencilDesc depthStencilDesc;
	depthStencilDesc.depthEnable = false;
	depthStencilDesc.depthWriteMask = false;

	this->depthStencilState_ = resourceManager.createDepthStencilState(depthStencilDesc);
	if (!this->depthStencilState_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create depth stencil state.");
		return false;
	}
	this->depthStencilState_->setDebugName("MyGUI::DepthStencilState");

	this->uvScaleBuffer_ = resourceManager.createConstantBuffer(sizeof(ConstantBuffer));
	if (!this->uvScaleBuffer_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create UV scale constant buffer.");
		return false;
	}
	this->uvScaleBuffer_->setDebugName("MyGUI::UvScaleBuffer");

	this->pipelineContext_ = resourceManager.createPipelineContext();
	if (!this->pipelineContext_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create pipeline context.");
		return false;
	}
	this->pipelineContext_->setDebugName("MyGUI::PipelineContext");

	this->shaderPlain.vertexShader = resourceManager.createShader<rend::VertexShader>("$RGE_DATA/Shaders/gui_mygui.hlsl", "vsMain");
	this->shaderPlain.pixelShader = resourceManager.createShader<rend::PixelShader>("$RGE_DATA/Shaders/gui_mygui.hlsl", "psMain");

	rend::InputLayoutDesc inputLayoutDescPlain;
	inputLayoutDescPlain.addElement("POSITION", 0, rend::Format::R32G32B32_FLOAT);
	inputLayoutDescPlain.addElement("COLOR", 0, rend::Format::R8G8B8A8_UNORM);

	this->shaderPlain.inputLayout = resourceManager.createInputLayout(inputLayoutDescPlain, this->shaderPlain.vertexShader);
	if (!this->shaderPlain.isValid()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create shader for untextured geometry.");
		return false;
	}
	this->shaderPlain.vertexShader->setDebugName("MyGUI::VertexShader (Plain)");
	this->shaderPlain.pixelShader->setDebugName("MyGUI::PixelShader (Plain)");
	this->shaderPlain.inputLayout->setDebugName("MyGUI::InputLayout (Plain)");

	this->shaderTextured.vertexShader = resourceManager.createShader<rend::VertexShader>("$RGE_DATA/Shaders/gui_mygui.hlsl", "vsMain", { "TEXTURED" });
	this->shaderTextured.pixelShader = resourceManager.createShader<rend::PixelShader>("$RGE_DATA/Shaders/gui_mygui.hlsl", "psMain", { "TEXTURED" });

	rend::InputLayoutDesc inputLayoutDescTextured;
	inputLayoutDescTextured.addElement("POSITION", 0, rend::Format::R32G32B32_FLOAT);
	inputLayoutDescTextured.addElement("COLOR", 0, rend::Format::R8G8B8A8_UNORM);
	inputLayoutDescTextured.addElement("TEXCOORD", 0, rend::Format::R32G32_FLOAT);

	this->shaderTextured.inputLayout = resourceManager.createInputLayout(inputLayoutDescTextured, this->shaderTextured.vertexShader);
	if (!this->shaderTextured.isValid()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::initialise: Failed to create shader for textured geometry.");
		return false;
	}
	this->shaderTextured.vertexShader->setDebugName("MyGUI::VertexShader (Textured)");
	this->shaderTextured.pixelShader->setDebugName("MyGUI::PixelShader (Textured)");
	this->shaderTextured.inputLayout->setDebugName("MyGUI::InputLayout (Textured)");

	return true;
}

void MyGUICustom::DirectX11RenderManager::shutdown() {
	this->destroyAllResources();

	this->shaderTextured.reset();
	this->shaderPlain.reset();

	this->uvScaleBuffer_.reset();
	this->pipelineContext_.reset();
	this->renderView_.reset();

	this->depthStencilState_.reset();
	this->rasterizerState_.reset();
	this->blendState_.reset();
	this->samplerState_.reset();
}

MyGUI::ITexture* MyGUICustom::DirectX11RenderManager::createTexture(const std::string& _name) {
	auto iterator = this->textures.find(_name);
	if (iterator != this->textures.end()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11RenderManager::createTexture: Texture with name '%s' already exists. Returning existing texture.", _name.c_str());
		return iterator->second;
	}

	MyGUICustom::DirectX11Texture* texture = new MyGUICustom::DirectX11Texture(_name);
	this->textures[_name] = texture;

	return texture;
}

void MyGUICustom::DirectX11RenderManager::destroyTexture(MyGUI::ITexture* _texture) {
	if (!_texture) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11RenderManager::destroyTexture: Attempted to destroy a null texture.");
		return;
	}

	auto iterator = this->textures.find(_texture->getName());
	if (iterator == this->textures.end()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11RenderManager::destroyTexture: Texture with name '%s' not found in manager.", _texture->getName().c_str());
		return;
	}

	this->textures.erase(iterator);
	delete _texture;
}

MyGUI::ITexture* MyGUICustom::DirectX11RenderManager::getTexture(const std::string& _name) {
	auto iterator = this->textures.find(_name);
	if (iterator != this->textures.end())
		return iterator->second;

	return nullptr;
}

void MyGUICustom::DirectX11RenderManager::begin() {
	if (!this->renderView_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::begin: No render view set.");
		return;
	}

	if (!this->pipelineContext_->begin())
		return;

	this->isActive_ = true;
	this->setupContext(*this->pipelineContext_, this->renderView_, static_cast<float>(this->viewSize.width), static_cast<float>(this->viewSize.height));
}

void MyGUICustom::DirectX11RenderManager::end() {
	if (!this->isActive_)
		return;

	this->isActive_ = false;

	if (!this->pipelineContext_->end())
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::end: Failed to end pipeline context.");
}

void MyGUICustom::DirectX11RenderManager::setupContext(rend::PipelineContext& context, rend::RenderView* renderView, float width, float height) {
	context.setRenderViews(renderView);

	rend::Viewport viewport;
	viewport.width = width;
	viewport.height = height;

	context.setViewports(viewport);

	context.setBlendState(this->blendState_);
	context.setRasterizerState(this->rasterizerState_);
	context.setDepthStencilState(this->depthStencilState_);
	context.setPrimitiveTopology(rend::PrimitiveTopology::TriangleList);
}

void MyGUICustom::DirectX11RenderManager::doRender(MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count) {
	if (this->isActive_)
		this->drawBatch(*this->pipelineContext_, _buffer, _texture, _count);
}

void MyGUICustom::DirectX11RenderManager::drawBatch(rend::PipelineContext& context, MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count) {
	if (!_buffer || _count == 0)
		return;

	const MyGUICustom::DirectX11VertexBuffer* buffer = static_cast<MyGUICustom::DirectX11VertexBuffer*>(_buffer);
	if (!buffer->getBuffer())
		return;

	MyGUICustom::DirectX11Texture* texture = static_cast<MyGUICustom::DirectX11Texture*>(_texture);
	const bool isTextured = texture && texture->getTexture();

	const Shader& shader = isTextured ? this->shaderTextured : this->shaderPlain;
	context.setInputLayout(shader.inputLayout);
	context.setVertexShader(shader.vertexShader);
	context.setPixelShader(shader.pixelShader);

	if (isTextured) {
		ConstantBuffer constants;
		constants.uvScale[0] = static_cast<float>(texture->getWidth()) / static_cast<float>(texture->getTextureWidth());
		constants.uvScale[1] = static_cast<float>(texture->getHeight()) / static_cast<float>(texture->getTextureHeight());

		if (!this->uvScaleBuffer_->writeRawBuffer(&constants, sizeof(ConstantBuffer)))
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RenderManager::drawBatch: Failed to update UV scale buffer.");

		context.setShaderResourceViews<rend::ShaderStage::Pixel>(texture->getTexture());
		context.setSamplerStates<rend::ShaderStage::Pixel>(this->samplerState_);
		context.setConstantBuffers<rend::ShaderStage::Pixel>(this->uvScaleBuffer_.get());
	} else {
		context.clearShaderResourceViews<rend::ShaderStage::Pixel>();
		context.clearSamplerStates<rend::ShaderStage::Pixel>();
		context.clearConstantBuffers<rend::ShaderStage::Pixel>();
	}

	context.setVertexBuffers(buffer->getBuffer().get());
	context.draw(static_cast<uint32_t>(_count), 0);
}

void MyGUICustom::DirectX11RenderManager::destroyAllResources() {
	for (auto& pair : this->textures)
		delete pair.second;

	this->textures.clear();
}

void MyGUICustom::DirectX11RenderManager::drawOneFrame() {
	MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
	if (!gui) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "DirectX11RenderManager::drawOneFrame: Failed to get GUI instance.");
		return;
	}

	uint32_t currentTime = this->timer_.getMilliseconds();
	uint32_t deltaTime = currentTime - this->lastTime_;

	this->onFrameEvent(static_cast<float>(deltaTime) / 1000.0f);
	this->lastTime_ = currentTime;

	this->begin();
	if (this->isActive_)
		this->onRenderToTarget(this, this->needsUpdate_);
	this->end();

	this->needsUpdate_ = false;
}

void MyGUICustom::DirectX11RenderManager::setViewSize(int _width, int _height) {
	_width = std::max(1, _width);
	_height = std::max(1, _height);

	this->viewSize.set(_width, _height);

	this->info.maximumDepth = 0.0f;
	this->info.hOffset = 0.0f;
	this->info.vOffset = 0.0f;
	this->info.aspectCoef = static_cast<float>(_height) / static_cast<float>(_width);
	this->info.pixScaleX = 1.0f / static_cast<float>(_width);
	this->info.pixScaleY = 1.0f / static_cast<float>(_height);

	this->onResizeView(this->viewSize);

	this->needsUpdate_ = true;
}