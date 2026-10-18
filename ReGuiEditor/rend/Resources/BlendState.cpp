#include "BlendState.hpp"

#include <rend/D3D11Device.hpp>

#include <Windows.h>
#include <d3d11.h>

bool rend::BlendState::init(const rend::BlendDesc& desc) {
	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
	blendDesc.AlphaToCoverageEnable = desc.alphaToCoverage;
	blendDesc.IndependentBlendEnable = desc.independentBlend;

	for (size_t index = 0; index < desc.renderTargets.size(); ++index) {
		const rend::BlendDesc::RenderTargetBlendDesc& inputRTDesc = desc.renderTargets[index];

		D3D11_RENDER_TARGET_BLEND_DESC& outputRTDesc = blendDesc.RenderTarget[index];
		outputRTDesc.BlendEnable = inputRTDesc.blendEnable;
		outputRTDesc.SrcBlend = static_cast<D3D11_BLEND>(inputRTDesc.srcBlend);
		outputRTDesc.DestBlend = static_cast<D3D11_BLEND>(inputRTDesc.destBlend);
		outputRTDesc.BlendOp = static_cast<D3D11_BLEND_OP>(inputRTDesc.blendOperand);
		outputRTDesc.SrcBlendAlpha = static_cast<D3D11_BLEND>(inputRTDesc.srcAlphaBlend);
		outputRTDesc.DestBlendAlpha = static_cast<D3D11_BLEND>(inputRTDesc.destAlphaBlend);
		outputRTDesc.BlendOpAlpha = static_cast<D3D11_BLEND_OP>(inputRTDesc.alphaBlendOperand);
		outputRTDesc.RenderTargetWriteMask = static_cast<UINT8>(inputRTDesc.renderTargetWriteMask);
	}

	HRESULT result = rend::D3D11Device::getInstance().getDevice1()->CreateBlendState(&blendDesc, &this->blendState_);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create blend state!");
		return false;
	}
	
	this->desc_ = desc;
	return true;
}