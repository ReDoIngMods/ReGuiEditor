#pragma once
#include <cstdint>

namespace rend {
	enum class StencilOperand : uint8_t {
		Keep = 1,           // D3D11_STENCIL_OP_KEEP
		Zero = 2,           // D3D11_STENCIL_OP_ZERO
		Replace = 3,        // D3D11_STENCIL_OP_REPLACE
		Invert = 6,         // D3D11_STENCIL_OP_INVERT
		IncrementClamp = 4, // D3D11_STENCIL_OP_INCR_SAT
		DecrementClamp = 5, // D3D11_STENCIL_OP_DECR_SAT
		IncrementWrap = 7,  // D3D11_STENCIL_OP_INCR
		DecrementWrap = 8   // D3D11_STENCIL_OP_DECR
	};

	inline static const char* stencilOperandToString(rend::StencilOperand stencilOperand) {
		switch (stencilOperand) {
		case rend::StencilOperand::Keep:            return "Keep";
		case rend::StencilOperand::Zero:            return "Zero";
		case rend::StencilOperand::Replace:         return "Replace";
		case rend::StencilOperand::Invert:          return "Invert";
		case rend::StencilOperand::IncrementClamp:  return "IncrementClamp";
		case rend::StencilOperand::DecrementClamp:  return "DecrementClamp";
		case rend::StencilOperand::IncrementWrap:   return "IncrementWrap";
		case rend::StencilOperand::DecrementWrap:   return "DecrementWrap";
		default: return "Unknown";
		}
	}
}