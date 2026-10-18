#include "DepthStencilState.hpp"

#include <rend/D3D11Device.hpp>

#include <Windows.h>
#include <d3d11.h>

inline static D3D11_DEPTH_STENCILOP_DESC convertStencilOpDesc(const rend::DepthStencilOpDesc& faceDesc) {
	D3D11_DEPTH_STENCILOP_DESC d3dFaceDesc;
	ZeroMemory(&d3dFaceDesc, sizeof(D3D11_DEPTH_STENCILOP_DESC));
	d3dFaceDesc.StencilFailOp = static_cast<D3D11_STENCIL_OP>(faceDesc.stencilFailOp);
	d3dFaceDesc.StencilDepthFailOp = static_cast<D3D11_STENCIL_OP>(faceDesc.stencilDepthFailOp);
	d3dFaceDesc.StencilPassOp = static_cast<D3D11_STENCIL_OP>(faceDesc.stencilPassOp);
	d3dFaceDesc.StencilFunc = static_cast<D3D11_COMPARISON_FUNC>(faceDesc.stencilFunc);

	return d3dFaceDesc;
}

bool rend::DepthStencilState::init(const rend::DepthStencilDesc& desc) {
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	depthStencilDesc.DepthEnable = desc.depthEnable;
	depthStencilDesc.DepthWriteMask = desc.depthWriteMask ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(desc.depthFunc);
	depthStencilDesc.StencilEnable = desc.stencilEnable;
	depthStencilDesc.StencilReadMask = desc.stencilReadMask;
	depthStencilDesc.StencilWriteMask = desc.stencilWriteMask;
	depthStencilDesc.FrontFace = convertStencilOpDesc(desc.frontFace);
	depthStencilDesc.BackFace = convertStencilOpDesc(desc.backFace);
	
	HRESULT result = rend::D3D11Device::getInstance().getDevice1()->CreateDepthStencilState(&depthStencilDesc, &this->depthStencilState_);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create depth stencil state! HRESULT: 0x%08X", result);
		return false;
	}

	this->desc_ = desc;
	return true;
}