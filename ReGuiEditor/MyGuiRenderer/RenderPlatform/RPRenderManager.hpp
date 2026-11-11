#pragma once
#include "RPVertexBuffer.hpp"

#include <rend/Common/RefCounter.hpp>

#include <rend/Resources/BlendState.hpp>
#include <rend/Resources/DepthStencilState.hpp>
#include <rend/Resources/RasterizerState.hpp>
#include <rend/Resources/SamplerState.hpp>

#include <rend/Resources/PipelineContext.hpp>

#include <rend/Resources/RenderView.hpp>

#include <rend/Resources/Shaders/InputLayout.hpp>
#include <rend/Resources/Shaders/PixelShader.hpp>
#include <rend/Resources/Shaders/VertexShader.hpp>

#include <MyGui/MyGUI_IRenderTarget.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_IVertexBuffer.h>
#include <MyGui/MyGUI_RenderFormat.h>
#include <MyGui/MyGUI_RenderManager.h>
#include <MyGui/MyGUI_RenderTargetInfo.h>
#include <MyGui/MyGUI_Timer.h>
#include <MyGui/MyGUI_Types.h>

#include <cstdint>
#include <map>
#include <string>

class RPTexture;

class RPRenderManager : public MyGUI::RenderManager, public MyGUI::IRenderTarget {
	rend::RefCountPtr<rend::SamplerState> samplerState_;
	rend::RefCountPtr<rend::BlendState> blendState_;
	rend::RefCountPtr<rend::RasterizerState> rasterizerState_;
	rend::RefCountPtr<rend::DepthStencilState> depthStencilState_;

	rend::RefCountPtr<rend::PipelineContext> pipelineContext_;

	struct Shader {
		rend::RefCountPtr<rend::VertexShader> vertexShader;
		rend::RefCountPtr<rend::PixelShader> pixelShader;
		rend::RefCountPtr<rend::InputLayout> inputLayout;

		bool isValid() const {
			return this->vertexShader && this->pixelShader && this->inputLayout;
		}

		void reset() {
			this->vertexShader.reset();
			this->pixelShader.reset();
			this->inputLayout.reset();
		}
	} shaderPlain, shaderTextured;

	std::map<std::string, Shader> registeredShaders_;

	const Shader& getTexturedShader(const RPTexture& texture) const;

	MyGUI::IntSize viewSize = MyGUI::IntSize(0, 0);
	MyGUI::VertexColourType vertexFormat = MyGUI::VertexColourType::ColourABGR;
	MyGUI::RenderTargetInfo info{};

	typedef std::map<std::string, MyGUI::ITexture*> MapTexture;
	MapTexture textures;

	rend::RefCountPtr<rend::RenderView> renderView_;

	MyGUI::Timer timer_;
	uint32_t lastTime_ = 0;

	bool needsUpdate_ = false;
	bool isActive_ = false;
public:
	bool initialise();
	void shutdown();

	static RPRenderManager& getInstance() {
		return *getInstancePtr();
	}

	static RPRenderManager* getInstancePtr() {
		return static_cast<RPRenderManager*>(MyGUI::RenderManager::getInstancePtr());
	}

	virtual MyGUI::IVertexBuffer* createVertexBuffer() override { return new RPVertexBuffer; }
	virtual void destroyVertexBuffer(MyGUI::IVertexBuffer* _buffer) override { delete _buffer; }

	virtual MyGUI::ITexture* createTexture(const std::string& _name) override;
	virtual void destroyTexture(MyGUI::ITexture* _texture) override;
	virtual MyGUI::ITexture* getTexture(const std::string& _name) override;

	virtual const MyGUI::IntSize& getViewSize() const override { return this->viewSize; }
	virtual MyGUI::VertexColourType getVertexFormat() const override { return this->vertexFormat; }
	virtual bool isFormatSupported(MyGUI::PixelFormat _format, MyGUI::TextureUsage _usage) override { return _format == MyGUI::PixelFormat::R8G8B8A8 || (_format == MyGUI::PixelFormat::L8 && !_usage.isValue(MyGUI::TextureUsage::RenderTarget)); }

	virtual void begin() override;
	virtual void end() override;

	virtual void doRender(MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count) override;

	virtual void registerShader(const std::string& _shaderName, const std::string& _vertexProgramFile, const std::string& _fragmentProgramFile) override;
	virtual const MyGUI::RenderTargetInfo& getInfo() const override { return this->info; }

	void setupContext(rend::PipelineContext& context, rend::RenderView* renderView, float width, float height);
	void drawBatch(rend::PipelineContext& context, MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count);

	void setRenderView(rend::RenderView* renderView) { this->renderView_ = renderView; }

	void drawOneFrame();
	void setViewSize(int _width, int _height) override;

	void destroyAllResources();
};