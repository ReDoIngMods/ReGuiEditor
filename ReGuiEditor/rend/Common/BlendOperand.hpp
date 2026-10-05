#pragma once
#include <cstdint>

namespace rend {
	enum class BlendOperand : uint8_t {
		Add = 1,         // D3D11_BLEND_OP_ADD
		Subtract = 2,    // D3D11_BLEND_OP_SUBTRACT
		RevSubtract = 3, // D3D11_BLEND_OP_REV_SUBTRACT
		Min = 4,         // D3D11_BLEND_OP_MIN
		Max = 5          // D3D11_BLEND_OP_MAX
	 };

	inline static const char* blendOperandToString(rend::BlendOperand blendOperand) {
		switch (blendOperand) {
		case rend::BlendOperand::Add:         return "Add";
		case rend::BlendOperand::Subtract:    return "Subtract";
		case rend::BlendOperand::RevSubtract: return "RevSubtract";
		case rend::BlendOperand::Min:         return "Min";
		case rend::BlendOperand::Max:         return "Max";
		default: return "Unknown";
		}
	}
}