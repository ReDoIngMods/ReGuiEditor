#pragma once
#include <rend/Common/RefCounter.hpp>

#include "ShaderResource.hpp"

#include <d3d11.h>

#include <string>
#include <vector>

namespace rend {
	class VertexShader : public rend::RefCounter<rend::ShaderResource> {
		rend::RefCountPtr<ID3D11VertexShader> vertexShader_;
	public:
		[[nodiscard]] bool init(const std::string& shaderName, const std::string& entrypoint, std::vector<std::string> macros) override;
		
		[[nodiscard]] rend::ShaderType getShaderType() const override { return rend::ShaderType::Vertex; }
		[[nodiscard]] rend::RefCountPtr<ID3D11VertexShader> getShader() const { return this->vertexShader_; }
	};

	typedef rend::RefCountPtr<rend::VertexShader> VertexShaderHandle;
}