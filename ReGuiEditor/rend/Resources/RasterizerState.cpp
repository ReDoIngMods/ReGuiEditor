#include "RasterizerState.hpp"

#include <rend/D3D11Device.hpp>

#include <Windows.h>
#include <d3d11.h>

bool rend::RasterizerState::init(const rend::RasterizerDesc& desc) {
	D3D11_RASTERIZER_DESC rasterizerDesc;
	ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
	rasterizerDesc.FillMode = desc.wireframeMode ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = static_cast<D3D11_CULL_MODE>(desc.cullMode);
	rasterizerDesc.FrontCounterClockwise = desc.advanced.frontCCW;
	rasterizerDesc.DepthBias = desc.advanced.depthBias;
	rasterizerDesc.DepthBiasClamp = desc.advanced.depthBiasClamp;
	rasterizerDesc.SlopeScaledDepthBias = desc.advanced.slopeScaledDepthBias;
	rasterizerDesc.DepthClipEnable = desc.depthClip;
	rasterizerDesc.ScissorEnable = desc.advanced.scissor;
	rasterizerDesc.MultisampleEnable = desc.multisample;
	rasterizerDesc.AntialiasedLineEnable = desc.antialiasedLines;
	
	HRESULT result = rend::D3D11Device::getInstance().getDevice1()->CreateRasterizerState(&rasterizerDesc, &this->rasterizerState_);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create rasterizer state!");
		return false;
	}
	
	this->desc_ = desc;
	return true;
}