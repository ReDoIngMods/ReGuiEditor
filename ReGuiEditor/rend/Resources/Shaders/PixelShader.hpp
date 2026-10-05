#pragma once
#include <rend/Common/RefCounter.hpp>

#include "ShaderResource.hpp"

#include <string>
#include <vector>

#include <d3d11.h>

namespace rend {
	class PixelShader : public rend::RefCounter<rend::ShaderResource> {
		rend::RefCountPtr<ID3D11PixelShader> pixelShader_;
	public:
		[[nodiscard]] bool init(const std::string& shaderName, const std::string& entrypoint, std::vector<std::string> macros) override;

		[[nodiscard]] rend::ShaderType getShaderType() const override { return rend::ShaderType::Pixel; }
		[[nodiscard]] rend::RefCountPtr<ID3D11PixelShader> getShader() const { return this->pixelShader_; }
	};

	typedef rend::RefCountPtr<rend::PixelShader> PixelShaderHandle;
}