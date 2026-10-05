#include "SamplerState.hpp"

#include <rend/D3D11Device.hpp>

#include <Windows.h>
#include <d3d11.h>

bool rend::SamplerState::init(const rend::SamplerDesc& desc) {
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));
	samplerDesc.Filter = static_cast<D3D11_FILTER>(desc.filter);
	samplerDesc.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.addressU);
	samplerDesc.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.addressV);
	samplerDesc.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.addressW);
	samplerDesc.MipLODBias = desc.mipLODBias;
	samplerDesc.MaxAnisotropy = desc.maxAnisotropy;
	samplerDesc.ComparisonFunc = static_cast<D3D11_COMPARISON_FUNC>(desc.comparisonFunc);
	samplerDesc.BorderColor[0] = desc.borderColor.r;
	samplerDesc.BorderColor[1] = desc.borderColor.g;
	samplerDesc.BorderColor[2] = desc.borderColor.b;
	samplerDesc.BorderColor[3] = desc.borderColor.a;
	samplerDesc.MinLOD = desc.minLOD;
	samplerDesc.MaxLOD = desc.maxLOD;
	
	HRESULT result = rend::D3D11Device::getInstance().getDevice1()->CreateSamplerState(&samplerDesc, &this->samplerState_);
	if (FAILED(result)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create sampler state!");
		return false;
	}
	
	this->desc_ = desc;
	return true;
}