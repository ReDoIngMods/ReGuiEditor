#include "RPRenderManager.hpp"

#include "RPTexture.hpp"
#include "RPVertexBuffer.hpp"

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
#include <utility>

struct alignas(16) ConstantBuffer {
	float uvScale[2] = { 1.0f, 1.0f };
};

static rend::RefCountPtr<rend::InputLayout> createGuiInputLayout(rend::ResourceManager& resourceManager, const rend::RefCountPtr<rend::VertexShader>& vertexShader, bool textured) {
	rend::InputLayoutDesc desc;
	desc.addElement("POSITION", 0, rend::Format::R32G32B32_FLOAT);
	desc.addElement("COLOR", 0, rend::Format::R8G8B8A8_UNORM);

	if (textured)
		desc.addElement("TEXCOORD", 0, rend::Format::R32G32_FLOAT);

	return resourceManager.createInputLayout(desc, vertexShader);
}

bool RPRenderManager::initialise() {
	this->vertexFormat = MyGUI::VertexColourType::ColourABGR;

	this->setViewSize(1920, 1080);

	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();
	this->samplerState_ = resourceManager.createLinearSamplerState(rend::SamplerAddressMode::Wrap);
	if (!this->samplerState_) {
		CONSOLE_ERROR("RPRenderManager::initialise: Failed to create sampler state.");
		return false;
	}
	this->samplerState_->setDebugName("MyGUI::SamplerState");

	this->blendState_ = resourceManager.createDefaultBlendState();
	if (!this->blendState_) {
		CONSOLE_ERROR("RPRenderManager::initialise: Failed to create default blend state.");
		return false;
	}
	this->blendState_->setDebugName("MyGUI::BlendState");

	// MyGUI quads are counter-clockwise, so back-face culling would discard them.
	rend::RasterizerDesc rasterizerDesc;
	rasterizerDesc.cullMode = rend::CullingMode::None;
	rasterizerDesc.depthClip = false;

	this->rasterizerState_ = resourceManager.createRasterizerState(rasterizerDesc);
	if (!this->rasterizerState_) {
		CONSOLE_ERROR("RPRenderManager::initialise: Failed to create rasterizer state.");
		return false;
	}
	this->rasterizerState_->setDebugName("MyGUI::RasterizerState");

	// MyGUI draws everything at the same depth and relies on draw order, so depth testing must be off.
	rend::DepthStencilDesc depthStencilDesc;
	depthStencilDesc.depthEnable = false;
	depthStencilDesc.depthWriteMask = false;

	this->depthStencilState_ = resourceManager.createDepthStencilState(depthStencilDesc);
	if (!this->depthStencilState_) {
		CONSOLE_ERROR("RPRenderManager::initialise: Failed to create depth stencil state.");
		return false;
	}
	this->depthStencilState_->setDebugName("MyGUI::DepthStencilState");

	this->uvScaleBuffer_ = resourceManager.createConstantBuffer(sizeof(ConstantBuffer));
	if (!this->uvScaleBuffer_) {
		CONSOLE_ERROR("RPRenderManager::initialise: Failed to create UV scale constant buffer.");
		return false;
	}
	this->uvScaleBuffer_->setDebugName("MyGUI::UvScaleBuffer");

	this->pipelineContext_ = resourceManager.createPipelineContext();
	if (!this->pipelineContext_) {
		CONSOLE_ERROR("RPRenderManager::initialise: Failed to create pipeline context.");
		return false;
	}
	this->pipelineContext_->setDebugName("MyGUI::PipelineContext");

	this->shaderPlain.vertexShader = resourceManager.createShader<rend::VertexShader>("$RGE_DATA/Shaders/gui_mygui.hlsl", "vsMain");
	this->shaderPlain.pixelShader = resourceManager.createShader<rend::PixelShader>("$RGE_DATA/Shaders/gui_mygui.hlsl", "psMain");

	this->shaderPlain.inputLayout = createGuiInputLayout(resourceManager, this->shaderPlain.vertexShader, false);
	if (!this->shaderPlain.isValid()) {
		CONSOLE_ERROR("RPRenderManager::initialise: Failed to create shader for untextured geometry.");
		return false;
	}
	this->shaderPlain.vertexShader->setDebugName("MyGUI::VertexShader (Plain)");
	this->shaderPlain.pixelShader->setDebugName("MyGUI::PixelShader (Plain)");
	this->shaderPlain.inputLayout->setDebugName("MyGUI::InputLayout (Plain)");

	this->shaderTextured.vertexShader = resourceManager.createShader<rend::VertexShader>("$RGE_DATA/Shaders/gui_mygui.hlsl", "vsMain", { "TEXTURED" });
	this->shaderTextured.pixelShader = resourceManager.createShader<rend::PixelShader>("$RGE_DATA/Shaders/gui_mygui.hlsl", "psMain", { "TEXTURED" });

	this->shaderTextured.inputLayout = createGuiInputLayout(resourceManager, this->shaderTextured.vertexShader, true);
	if (!this->shaderTextured.isValid()) {
		CONSOLE_ERROR("RPRenderManager::initialise: Failed to create shader for textured geometry.");
		return false;
	}
	this->shaderTextured.vertexShader->setDebugName("MyGUI::VertexShader (Textured)");
	this->shaderTextured.pixelShader->setDebugName("MyGUI::PixelShader (Textured)");
	this->shaderTextured.inputLayout->setDebugName("MyGUI::InputLayout (Textured)");

	return true;
}

void RPRenderManager::shutdown() {
	this->destroyAllResources();

	this->registeredShaders_.clear();
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

MyGUI::ITexture* RPRenderManager::createTexture(const std::string& _name) {
	auto iterator = this->textures.find(_name);
	if (iterator != this->textures.end()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "RPRenderManager::createTexture: Texture with name '%s' already exists. Returning existing texture.", _name.c_str());
		return iterator->second;
	}

	RPTexture* texture = new RPTexture(_name);
	this->textures[_name] = texture;

	return texture;
}

void RPRenderManager::destroyTexture(MyGUI::ITexture* _texture) {
	if (!_texture) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "RPRenderManager::destroyTexture: Attempted to destroy a null texture.");
		return;
	}

	auto iterator = this->textures.find(_texture->getName());
	if (iterator == this->textures.end()) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "RPRenderManager::destroyTexture: Texture with name '%s' not found in manager.", _texture->getName().c_str());
		return;
	}

	this->textures.erase(iterator);
	delete _texture;
}

MyGUI::ITexture* RPRenderManager::getTexture(const std::string& _name) {
	auto iterator = this->textures.find(_name);
	if (iterator != this->textures.end())
		return iterator->second;

	return nullptr;
}

void RPRenderManager::registerShader(const std::string& _shaderName, const std::string& _vertexProgramFile, const std::string& _fragmentProgramFile) {
	rend::ResourceManager& resourceManager = rend::ResourceManager::getInstance();

	Shader shader;
	shader.vertexShader = resourceManager.createShader<rend::VertexShader>(_vertexProgramFile, "vsMain");
	shader.pixelShader = resourceManager.createShader<rend::PixelShader>(_fragmentProgramFile, "psMain");
	shader.inputLayout = createGuiInputLayout(resourceManager, shader.vertexShader, true);

	if (!shader.isValid()) {
		CONSOLE_ERROR("RPRenderManager::registerShader: Failed to create shader '%s' (%s, %s).", _shaderName.c_str(), _vertexProgramFile.c_str(), _fragmentProgramFile.c_str());
		return;
	}

	shader.vertexShader->setDebugName("MyGUI::VertexShader (" + _shaderName + ")");
	shader.pixelShader->setDebugName("MyGUI::PixelShader (" + _shaderName + ")");
	shader.inputLayout->setDebugName("MyGUI::InputLayout (" + _shaderName + ")");

	if (_shaderName == "Default")
		this->shaderTextured = std::move(shader);
	else
		this->registeredShaders_[_shaderName] = std::move(shader);
}

auto RPRenderManager::getTexturedShader(const RPTexture& texture) const -> const Shader& {
	const std::string& shaderName = texture.getShaderName();
	if (shaderName.empty())
		return this->shaderTextured;

	const auto iterator = this->registeredShaders_.find(shaderName);
	return iterator != this->registeredShaders_.end() ? iterator->second : this->shaderTextured;
}

void RPRenderManager::begin() {
	if (!this->renderView_) {
		CONSOLE_ERROR("RPRenderManager::begin: No render view set.");
		return;
	}

	if (!this->pipelineContext_->begin())
		return;

	this->isActive_ = true;
	this->setupContext(*this->pipelineContext_, this->renderView_, static_cast<float>(this->viewSize.width), static_cast<float>(this->viewSize.height));
}

void RPRenderManager::end() {
	if (!this->isActive_)
		return;

	this->isActive_ = false;

	if (!this->pipelineContext_->end())
		CONSOLE_ERROR("RPRenderManager::end: Failed to end pipeline context.");
}

void RPRenderManager::setupContext(rend::PipelineContext& context, rend::RenderView* renderView, float width, float height) {
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

void RPRenderManager::doRender(MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count) {
	if (this->isActive_)
		this->drawBatch(*this->pipelineContext_, _buffer, _texture, _count);
}

void RPRenderManager::drawBatch(rend::PipelineContext& context, MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count) {
	if (!_buffer || _count == 0)
		return;

	const RPVertexBuffer* buffer = static_cast<RPVertexBuffer*>(_buffer);
	if (!buffer->getBuffer())
		return;

	RPTexture* texture = static_cast<RPTexture*>(_texture);
	const bool isTextured = texture && texture->getTexture();

	const Shader& shader = isTextured ? this->getTexturedShader(*texture) : this->shaderPlain;
	context.setInputLayout(shader.inputLayout);
	context.setVertexShader(shader.vertexShader);
	context.setPixelShader(shader.pixelShader);

	if (isTextured) {
		ConstantBuffer constants;
		constants.uvScale[0] = static_cast<float>(texture->getWidth()) / static_cast<float>(texture->getTextureWidth());
		constants.uvScale[1] = static_cast<float>(texture->getHeight()) / static_cast<float>(texture->getTextureHeight());

		if (!this->uvScaleBuffer_->writeRawBuffer(&constants, sizeof(ConstantBuffer)))
			CONSOLE_ERROR("RPRenderManager::drawBatch: Failed to update UV scale buffer.");

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

void RPRenderManager::destroyAllResources() {
	for (auto& pair : this->textures)
		delete pair.second;

	this->textures.clear();
}

void RPRenderManager::drawOneFrame() {
	MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
	if (!gui) {
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "RPRenderManager::drawOneFrame: Failed to get GUI instance.");
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

void RPRenderManager::setViewSize(int _width, int _height) {
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