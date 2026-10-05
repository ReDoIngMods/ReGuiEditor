#pragma once
#include "Resource.hpp"

#include <rend/Common/RefCounter.hpp>

#include <rend/Common/CullingMode.hpp>

#include <d3d11.h>

namespace rend {
	struct RasterizerDescEx {
		bool frontCCW = false;
		bool scissor = false;

		int depthBias = 0;
		float depthBiasClamp = 0.f;
		float slopeScaledDepthBias = 0.f;
	};

	struct RasterizerDesc {
		rend::CullingMode cullMode = rend::CullingMode::Back;

		bool wireframeMode = false;
		bool depthClip = true;
		bool multisample = false;
		bool antialiasedLines = false;

		rend::RasterizerDescEx advanced;
	};

	class RasterizerState : public rend::RefCounter<rend::Resource> {
		rend::RefCountPtr<ID3D11RasterizerState> rasterizerState_;

		rend::RasterizerDesc desc_;
	public:
		[[nodiscard]] bool init(const rend::RasterizerDesc& desc);

		[[nodiscard]] rend::RasterizerDesc getDesc() const { return this->desc_; }
		[[nodiscard]] rend::RefCountPtr<ID3D11RasterizerState> getRasterizerState() const { return this->rasterizerState_; }
	};

	typedef rend::RefCountPtr<rend::RasterizerState> RasterizerStateHandle;
}