#pragma once

namespace rend {
	enum class BlendMode {
		Zero = 1,          // D3D11_BLEND_ZERO
		One = 2,           // D3D11_BLEND_ONE
		SrcColor = 3,      // D3D11_BLEND_SRC_COLOR
		InvSrcColor = 4,   // D3D11_BLEND_INV_SRC_COLOR
		SrcAlpha = 5,      // D3D11_BLEND_SRC_ALPHA
		InvSrcAlpha = 6,   // D3D11_BLEND_INV_SRC_ALPHA
		DestAlpha = 7,     // D3D11_BLEND_DEST_ALPHA
		InvDestAlpha = 8,  // D3D11_BLEND_INV_DEST_ALPHA
		DestColor = 9,     // D3D11_BLEND_DEST_COLOR
		InvDestColor = 10, // D3D11_BLEND_INV_DEST_COLOR
		SrcAlphaSat = 11   // D3D11_BLEND_SRC_ALPHA_SAT
	};

	inline static const char* blendModeToString(rend::BlendMode blendMode) {
		switch (blendMode) {
		case rend::BlendMode::Zero:         return "Zero";
		case rend::BlendMode::One:          return "One";
		case rend::BlendMode::SrcColor:     return "SrcColor";
		case rend::BlendMode::InvSrcColor:  return "InvSrcColor";
		case rend::BlendMode::SrcAlpha:     return "SrcAlpha";
		case rend::BlendMode::InvSrcAlpha:  return "InvSrcAlpha";
		case rend::BlendMode::DestAlpha:    return "DestAlpha";
		case rend::BlendMode::InvDestAlpha: return "InvDestAlpha";
		case rend::BlendMode::DestColor:    return "DestColor";
		case rend::BlendMode::InvDestColor: return "InvDestColor";
		case rend::BlendMode::SrcAlphaSat:  return "SrcAlphaSat";
		default: return "Unknown";
		}
	}
}