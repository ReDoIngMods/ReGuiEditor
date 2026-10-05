#pragma once
#include "Resource.hpp"

#include <rend/Common/RefCounter.hpp>

#include <rend/Common/Color.hpp>
#include <rend/Common/ComparisonFunction.hpp>
#include <rend/Common/SamplerAddressMode.hpp>
#include <rend/Common/SamplerFilter.hpp>

#include <cfloat>
#include <cstdint>

#include <d3d11.h>

namespace rend {
	struct SamplerDesc {
		rend::SamplerFilter filter = SamplerFilter::Linear;
		rend::SamplerAddressMode addressU = rend::SamplerAddressMode::Wrap;
		rend::SamplerAddressMode addressV = rend::SamplerAddressMode::Wrap;
		rend::SamplerAddressMode addressW = rend::SamplerAddressMode::Wrap;
		
		float mipLODBias = 0.f;
		uint32_t maxAnisotropy = 1;
		rend::ComparisonFunction comparisonFunc = rend::ComparisonFunction::Always;

		rend::Color borderColor = rend::Color::transparent();
		float minLOD = -FLT_MAX;
		float maxLOD = FLT_MAX;
	};

	class SamplerState : public rend::RefCounter<rend::Resource> {
		rend::RefCountPtr<ID3D11SamplerState> samplerState_;

		rend::SamplerDesc desc_;
	public:
		[[nodiscard]] bool init(const rend::SamplerDesc& desc);

		[[nodiscard]] rend::SamplerDesc getDesc() const { return this->desc_; }
		[[nodiscard]] rend::RefCountPtr<ID3D11SamplerState> getSamplerState() const { return this->samplerState_; }
	};

	typedef rend::RefCountPtr<rend::SamplerState> SamplerStateHandle;
}