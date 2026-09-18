#pragma once
#include <MyGUI/MyGUI_IRenderTarget.h>
#include <MyGUI/MyGUI_ITexture.h>
#include <MyGui/MyGUI_IVertexBuffer.h>
#include <MyGui/MyGUI_RenderTargetInfo.h>

#include <d3d11.h>

namespace MyGUICustom {
	class DirectX11Texture;

	class DirectX11RTTexture : public MyGUI::IRenderTarget {
		ID3D11DepthStencilView* cachedDepthStencil_ = nullptr;
		ID3D11RenderTargetView* cachedRenderTarget_ = nullptr;
		D3D11_VIEWPORT cachedViewport_ = {};
	public:
		DirectX11Texture* texture = nullptr;
		ID3D11RenderTargetView* renderTarget = nullptr;
		MyGUI::RenderTargetInfo renderTargetInfo{};

		DirectX11RTTexture(DirectX11Texture* texture);
		virtual ~DirectX11RTTexture();

		virtual void begin();
		virtual void end();

		virtual void doRender(MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count);

		virtual const MyGUI::RenderTargetInfo& getInfo() {
			return renderTargetInfo;
		}
	};
}