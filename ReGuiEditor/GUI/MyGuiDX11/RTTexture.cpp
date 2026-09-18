#include "RTTexture.hpp"
#include "Texture.hpp"

#include "RenderManager.hpp"

#include <MyGui/MyGUI_ITexture.h>
#include <MyGui/MyGUI_IVertexBuffer.h>

#include <Windows.h>
#include <d3d11.h>
#include <dxgiformat.h>

MyGUICustom::DirectX11RTTexture::DirectX11RTTexture(MyGUICustom::DirectX11Texture* texture) : texture(texture) {
	int width = this->texture->getWidth();
	int height = this->texture->getHeight();

	this->renderTargetInfo.maximumDepth = 0.0f;
	this->renderTargetInfo.hOffset = 0.0f;
	this->renderTargetInfo.vOffset = 0.0f;
	this->renderTargetInfo.aspectCoef = float(height) / float(width);
	this->renderTargetInfo.pixScaleX = 1.0f / float(width);
	this->renderTargetInfo.pixScaleY = 1.0f / float(height);

	D3D11_RENDER_TARGET_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipSlice = 0;

	MyGUICustom::DirectX11RenderManager& renderManager = MyGUICustom::DirectX11RenderManager::getInstance();
	
	HRESULT result = renderManager.device->CreateRenderTargetView(this->texture->texture, 0, &this->renderTarget);
	if (FAILED(result))
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "DirectX11RTTexture::DirectX11RTTexture: Failed to create render target view. HRESULT: 0x%X", result);
}

MyGUICustom::DirectX11RTTexture::~DirectX11RTTexture() {
	if (this->renderTarget) {
		this->renderTarget->Release();
		this->renderTarget = nullptr;
	}
}

void MyGUICustom::DirectX11RTTexture::begin() {
	MyGUICustom::DirectX11RenderManager& renderManager = MyGUICustom::DirectX11RenderManager::getInstance();
	ID3D11DeviceContext* context = renderManager.context;

	UINT numViewports = 1;
	context->RSGetViewports(&numViewports, &this->cachedViewport_);
	context->OMGetRenderTargets(1, &this->cachedRenderTarget_, &this->cachedDepthStencil_);

	D3D11_VIEWPORT viewport;
	viewport.Width = static_cast<float>(this->texture->getWidth());
	viewport.Height = static_cast<float>(this->texture->getHeight());
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	context->RSSetViewports(1, &viewport);

	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	context->OMSetRenderTargets(1, &this->renderTarget, 0);
	context->ClearRenderTargetView(this->renderTarget, clearColor);
}

void MyGUICustom::DirectX11RTTexture::end() {
	MyGUICustom::DirectX11RenderManager& renderManager = MyGUICustom::DirectX11RenderManager::getInstance();
	
	ID3D11DeviceContext* context = renderManager.context;
	context->OMSetRenderTargets(1, &this->cachedRenderTarget_, this->cachedDepthStencil_);
	context->RSSetViewports(1, &this->cachedViewport_);
	
	if (this->cachedRenderTarget_) {
		this->cachedRenderTarget_->Release();
		this->cachedRenderTarget_ = nullptr;
	}
	
	if (this->cachedDepthStencil_) {
		this->cachedDepthStencil_->Release();
		this->cachedDepthStencil_ = nullptr;
	}
}

void MyGUICustom::DirectX11RTTexture::doRender(MyGUI::IVertexBuffer* _buffer, MyGUI::ITexture* _texture, size_t _count) {
	MyGUICustom::DirectX11RenderManager::getInstance().doRender(_buffer, _texture, _count);
}