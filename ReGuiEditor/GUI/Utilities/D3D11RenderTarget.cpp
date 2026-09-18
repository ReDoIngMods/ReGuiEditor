#include "D3D11RenderTarget.hpp" // VCIC-Excluded: Includer cleaner is being a dum dum

#include "../../Application.hpp"

#include <Windows.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgiformat.h>

bool RenderTargetUtils::createColorfulRTV(UINT width, UINT height, DXGI_FORMAT format, ID3D11Texture2D*& outputTexture, ID3D11RenderTargetView*& outputRTV, ID3D11ShaderResourceView*& outputSRV) {
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	ID3D11Device* device = Application::getInstance().d3d11Device.getDevice();
	HRESULT result = device->CreateTexture2D(&desc, nullptr, &outputTexture);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "RenderTargetUtils::createColorfulRTV: Failed to create texture. HRESULT: 0x%X", result);
		return false;
	}

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	ZeroMemory(&rtvDesc, sizeof(rtvDesc));
	rtvDesc.Format = desc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	result = device->CreateRenderTargetView(outputTexture, &rtvDesc, &outputRTV);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "RenderTargetUtils::createColorfulRTV: Failed to create RTV. HRESULT: 0x%X", result);
		
		outputTexture->Release();
		outputTexture = nullptr;

		return false;
	}
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	result = device->CreateShaderResourceView(outputTexture, &srvDesc, &outputSRV);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "RenderTargetUtils::createColorfulRTV: Failed to create SRV. HRESULT: 0x%X", result);
	
		outputRTV->Release();
		outputRTV = nullptr;

		outputTexture->Release();
		outputTexture = nullptr;

		return false;
	}

	return true;
}