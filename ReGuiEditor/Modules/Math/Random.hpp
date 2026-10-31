#pragma once
#include <array>
#include <cstdint>

namespace ReGuiEditor {
	float randomGenerateFloat(float min = 0.0, float max = 1.0);
	float normalRandomGenerateFloat(float mean = 0.0, float deviation = 1.0);

	std::array<uint8_t, 512> generatePermutationTableU8();
}