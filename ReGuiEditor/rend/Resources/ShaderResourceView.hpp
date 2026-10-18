#pragma once
#include "Resource.hpp"

#include <rend/Common/RefCounter.hpp>

#include <d3d11.h>

namespace rend {
	class ShaderResourceView : public rend::Resource {
	protected:
		bool isShaderResourceViewSupported_ = false;
		rend::RefCountPtr<ID3D11ShaderResourceView> shaderResourceView_ = nullptr;
	public:
		[[nodiscard]] rend::RefCountPtr<ID3D11ShaderResourceView> getShaderResourceView() const {
			if (!this->isShaderResourceViewSupported_) {
				CONSOLE_ERROR("Shader resource view is not supported for this resource");
				return nullptr;
			}

			return this->shaderResourceView_;
		}

		[[nodiscard]] bool isShaderResourceViewAvailable() const { return this->isShaderResourceViewSupported_; }
	};
}