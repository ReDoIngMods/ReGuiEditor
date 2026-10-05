#include "RenderView.hpp"

#include <rend/Common/RefCounter.hpp>

#include <rend/D3D11Device.hpp>

#include "ShaderResourceView.hpp"
#include "Texture2D.hpp"

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>

bool rend::RenderView::init(rend::Texture2D* texture) {
	if (!texture) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Texture is null");
		return false;
	}
	
	rend::RefCountPtr<ID3D11Device1> device = rend::D3D11Device::getInstance().getDevice1();
	HRESULT result = device->CreateRenderTargetView(texture->getTexture(), nullptr, &this->renderTargetView_);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create render target view! HRESULT: 0x%08X", result);
		return false;
	}

	this->isShaderResourceViewSupported_ = false;
	if (texture->getFlags() & rend::Texture2DFlags::ShaderResource) {
		result = device->CreateShaderResourceView(texture->getTexture(), nullptr, &this->shaderResourceView_);
		if (FAILED(result)) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create shader resource view! HRESULT: 0x%08X", result);
	
			this->renderTargetView_.reset();
			return false;
		}

		this->isShaderResourceViewSupported_ = true;
	}

	this->texture_ = texture;
	return true;
}