#pragma once
#include <cstdint>

namespace rend {
	enum class SamplerFilter : uint8_t {
		Point = 0,       // D3D11_FILTER_MIN_MAG_MIP_POINT
		Linear = 21,     // D3D11_FILTER_MIN_MAG_MIP_LINEAR
		Anisotropic = 85 // D3D11_FILTER_ANISOTROPIC
	};

	inline static const char* samplerFilterToString(rend::SamplerFilter samplerFilter) {
		switch (samplerFilter) {
		case rend::SamplerFilter::Point:       return "Point";
		case rend::SamplerFilter::Linear:      return "Linear";
		case rend::SamplerFilter::Anisotropic: return "Anisotropic";
		default: return "Unknown";
		}
	}
}