#pragma once
#include <rend/Common/RefCounter.hpp>

#include <rend/Resources/Resource.hpp>

#include "ShaderBlob.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rend {
	enum class ShaderType : uint8_t {
		Vertex,
		Pixel
	};

	class ShaderResource : public rend::Resource {
	protected:
		rend::ShaderBlobHandle shaderBlob_ = nullptr;
	public:
		[[nodiscard]] virtual bool init(const std::string& shaderName, const std::string& entrypoint, std::vector<std::string> macros) = 0;
		[[nodiscard]] virtual rend::ShaderType getShaderType() const = 0;

		[[nodiscard]] rend::ShaderBlobHandle getShaderBlob() const { return this->shaderBlob_; }
	};

	typedef rend::RefCountPtr<rend::ShaderResource> ShaderResourceHandle;
}