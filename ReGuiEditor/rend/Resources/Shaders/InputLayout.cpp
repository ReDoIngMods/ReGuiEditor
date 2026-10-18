#include "InputLayout.hpp"

#include <rend/D3D11Device.hpp>

#include "ShaderBlob.hpp"
#include "VertexShader.hpp"

#include <Windows.h>
#include <d3d11.h>
#include <d3dcommon.h>

bool rend::InputLayout::init(const rend::InputLayoutDesc& inputElementDescs, rend::VertexShader* vertexShader) {
	if (!vertexShader) {
		CONSOLE_ERROR("Vertex shader is null!");
		return false;
	}

	ID3DBlob* rawBlob = vertexShader->getShaderBlob()->getBlob().get();
	HRESULT result = rend::D3D11Device::getInstance().getDevice1()->CreateInputLayout(inputElementDescs.inputElementDescs.data(), static_cast<UINT>(inputElementDescs.inputElementDescs.size()), rawBlob->GetBufferPointer(), rawBlob->GetBufferSize(), &this->inputLayout_);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create input layout! HRESULT: 0x%X", result);
		return false;
	}

	this->inputLayoutDesc_ = inputElementDescs;
	this->vertexShader_ = vertexShader;
	return true;
}