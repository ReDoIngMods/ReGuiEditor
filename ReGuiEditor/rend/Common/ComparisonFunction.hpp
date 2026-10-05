#pragma once

namespace rend {
	enum class ComparisonFunction {
		Never = 1,        // D3D11_COMPARISON_NEVER
		Less = 2,         // D3D11_COMPARISON_LESS
		Equal = 3,        // D3D11_COMPARISON_EQUAL
		LessEqual = 4,    // D3D11_COMPARISON_LESS_EQUAL
		Greater = 5,      // D3D11_COMPARISON_GREATER
		NotEqual = 6,     // D3D11_COMPARISON_NOT_EQUAL
		GreaterEqual = 7, // D3D11_COMPARISON_GREATER_EQUAL
		Always = 8        // D3D11_COMPARISON_ALWAYS
	};

	inline static const char* comparisonFunctionToString(rend::ComparisonFunction comparisonFunction) {
		switch (comparisonFunction) {
		case rend::ComparisonFunction::Never:        return "Never";
		case rend::ComparisonFunction::Less:         return "Less";
		case rend::ComparisonFunction::Equal:        return "Equal";
		case rend::ComparisonFunction::LessEqual:    return "LessEqual";
		case rend::ComparisonFunction::Greater:      return "Greater";
		case rend::ComparisonFunction::NotEqual:     return "NotEqual";
		case rend::ComparisonFunction::GreaterEqual: return "GreaterEqual";
		case rend::ComparisonFunction::Always:       return "Always";
		default: return "Unknown";
		}
	}
}