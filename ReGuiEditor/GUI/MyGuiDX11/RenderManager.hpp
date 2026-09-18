#pragma once
#include "VertexBuffer.hpp"

#include <MyGui/MyGUI_IRenderTarget.h>
#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_IVertexBuffer.h>
#include <MyGui/MyGUI_RenderFormat.h>
#include <MyGui/MyGUI_RenderManager.h>
#include <MyGui/MyGUI_RenderTargetInfo.h>
#include <MyGui/MyGUI_Timer.h>
#include <MyGui/MyGUI_Types.h>

#include <d3d11.h>

#include <cstdint>
#include <map>
#include <string>

namespace MyGUICustom {
	class DirectX11RenderManager : public MyGUI::RenderManager, public MyGUI::IRenderTarget {
	public:
		ID3D11Device* device = nullptr;
		ID3D11DeviceContext* context = nullptr;
	private:
		ID3D11SamplerState* samplerState = nullptr;
		ID3D11BlendState* blendState = nullptr;
		ID3D11RasterizerState* rasterizerState = nullptr;
		ID3D11DepthStencilState* depthStencilState = nullptr;

		ID3D11Buffer* constantBuffer = nullptr;

		struct Shader {
			ID3D11VertexShader* vertexShader = nullptr;
			ID3D11PixelShader* pixelShader = nullptr;
			ID3D11InputLayout* inputLayout = nullptr;
		};

		Shader shaderPlain;
		Shader shaderTextured;

		MyGUI::IntSize viewSize = MyGUI::IntSize(0, 0);
		MyGUI::VertexColourType vertexFormat = MyGUI::VertexColourType::ColourABGR;
		MyGUI::RenderTargetInfo info{};

		typedef std::map<std::string, MyGUI::ITexture*> MapTexture;
		MapTexture textures;

		MyGUI::Timer timer_;
		uint32_t lastTime_ = 0;

		bool needsUpdate_ = false;
	public:
		bool initialise(ID3D11Device* device);
		void shutdown();

		static MyGUICustom::DirectX11RenderManager& getInstance() {
			return *getInstancePtr();
		}

		static MyGUICustom::DirectX11RenderManager* getInstancePtr() {
			return static_cast<MyGUICustom::DirectX11RenderManager*>(MyGUI::RenderManager::getInstancePtr());
		}

		virtual const MyGUI::IntSize& getViewSize() const override { return this->viewSize; }

		virtual MyGUI::VertexColourType getVertexFormat() override { return this->vertexFormat; }
		virtual MyGUI::IVertexBuffer* createVertexBuffer() override { return new MyGUICustom::DirectX11VertexBuffer; }

		virtual void destroyVertexBuffer(MyGUI::IVertexBuffer* _buffer) override { delete _buffer; }

		virtual MyGUI::ITexture* createTexture(const std::string& _name) override;
		virtual void destroyTexture(MyGUI::ITexture* _texture) override;
		virtual MyGUI::ITexture* getTexture(const std::string& _name) override;

		virtual bool isFormatSupported(MyGUI::PixelFormat _format, MyGUI::TextureUsage _usage) override {
			return _format == MyGUI::PixelFormat::R8G8B8A8;
		}

		virtual void begin() override;
		virtual void end() override;

		virtual void doRender(MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count) override;

		virtual const MyGUI::RenderTargetInfo& getInfo() override {
			return this->info;
		}

		void drawOneFrame();
		void setViewSize(int _width, int _height);

		void destroyAllResources();
	};
}
