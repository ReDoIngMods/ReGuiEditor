#include "PixelShader.hpp"

#include "ShaderResource.hpp"

#include <rend/Common/RefCounter.hpp>

#include <rend/D3D11Device.hpp>
#include <rend/ShaderManager.hpp>

#include <Windows.h>
#include <d3d11.h>
#include <d3dcommon.h>

#include <string>
#include <vector>

bool rend::PixelShader::init(const std::string& shaderName, const std::string& entrypoint, std::vector<std::string> macros) {
	if (!rend::ShaderManager::getInstance().loadBlob(shaderName, entrypoint, macros, rend::ShaderType::Pixel, &this->shaderBlob_)) {
		CONSOLE_ERROR("Failed to find or compile pixel shader: %s", shaderName.c_str());
		return false;
	}

	rend::RefCountPtr<ID3DBlob> rawBlob = this->shaderBlob_->getBlob();
	HRESULT result = rend::D3D11Device::getInstance().getDevice1()->CreatePixelShader(rawBlob->GetBufferPointer(), rawBlob->GetBufferSize(), nullptr, &this->pixelShader_);
	if (FAILED(result)) {
		CONSOLE_ERROR("Failed to create pixel shader: %s", shaderName.c_str());
		return false;
	}
	
	return true;
}