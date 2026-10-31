#include "Random.hpp" // VCIC-Excluded

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <random>

static std::mt19937& generator() {
	static thread_local std::mt19937 gen(std::random_device{}());
	return gen;
}

float ReGuiEditor::randomGenerateFloat(float min, float max) {
    return std::uniform_real_distribution<float>(min, max)(generator());
}

float ReGuiEditor::normalRandomGenerateFloat(float mean, float deviation) {
    return std::normal_distribution<float>(mean, deviation)(generator());
}

std::array<uint8_t, 512> ReGuiEditor::generatePermutationTableU8() {
	std::array<uint8_t, 512> result{};
    std::iota(result.begin(), result.begin() + 256, static_cast<uint8_t>(0));
	std::shuffle(result.begin(), result.begin() + 256, generator());
    std::copy_n(result.begin(), 256, result.begin() + 256);
	return result;
}