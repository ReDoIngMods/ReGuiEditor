#pragma once
#include <cstdint>

namespace rend {
	enum class CullingMode : uint8_t {
		None = 1,  // D3D11_CULL_NONE
		Front = 2, // D3D11_CULL_FRONT
		Back = 3   // D3D11_CULL_BACK
	};

	inline static const char* cullingModeToString(rend::CullingMode cullingMode) {
		switch (cullingMode) {
		case rend::CullingMode::None:  return "None";
		case rend::CullingMode::Front: return "Front";
		case rend::CullingMode::Back:  return "Back";
		default: return "Unknown";
		}
	}
}