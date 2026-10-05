#pragma once
#include <cstdint>

namespace rend {
	enum class SamplerAddressMode : uint8_t {
		Wrap = 1,      // D3D11_TEXTURE_ADDRESS_WRAP
		Mirror = 2,    // D3D11_TEXTURE_ADDRESS_MIRROR
		Clamp = 3,     // D3D11_TEXTURE_ADDRESS_CLAMP
		Border = 4,    // D3D11_TEXTURE_ADDRESS_BORDER
		MirrorOnce = 5 // D3D11_TEXTURE_ADDRESS_MIRROR_ONCE
	};

	inline static const char* samplerAddressModeToString(rend::SamplerAddressMode samplerAddressMode) {
		switch (samplerAddressMode) {
		case rend::SamplerAddressMode::Wrap:       return "Wrap";
		case rend::SamplerAddressMode::Mirror:     return "Mirror";
		case rend::SamplerAddressMode::Clamp:      return "Clamp";
		case rend::SamplerAddressMode::Border:     return "Border";
		case rend::SamplerAddressMode::MirrorOnce: return "MirrorOnce";
		default: return "Unknown";
		}
	}
}