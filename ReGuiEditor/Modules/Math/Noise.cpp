#include "Noise.hpp" // VCIC-Excluded

#include "Easing.hpp"
#include "Random.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

static thread_local const std::array<uint8_t, 512> PERMUTATION = ReGuiEditor::generatePermutationTableU8();

inline static constexpr uint32_t finalize(uint32_t hash) {
	hash ^= hash >> 16;
	hash *= 0x85EBCA6Bu;
	hash ^= hash >> 13;
	hash *= 0xC2B2AE35u;
	hash ^= hash >> 16;
	return hash;
}

uint32_t ReGuiEditor::Noise::hash2d(int32_t x, int32_t y, int32_t seed) {
	const uint32_t hashedSeed = finalize(static_cast<uint32_t>(seed));
	const uint32_t hashedY = finalize(static_cast<uint32_t>(y) * 0x85EBCA77u ^ hashedSeed);

	return finalize(static_cast<uint32_t>(x) * 0x9E3779B1u ^ hashedY);
}

float ReGuiEditor::Noise::perlinGradient(uint32_t hash, float x, float y) {
	switch (hash & 0x3) {
	case 0: return  x + y;
	case 1: return -x + y;
	case 2: return  x - y;
	case 3: return -x - y;
	default: return 0.0f; // Should never happen
	}
}

float ReGuiEditor::Noise::getPerlinCornerValue(int cellX, int cellY, int offsetX, int offsetY, float localX, float localY, int32_t seed) {
	const uint32_t hash = hash2d(cellX + offsetX, cellY + offsetY, seed);
	return perlinGradient(hash, localX - static_cast<float>(offsetX), localY - static_cast<float>(offsetY));
}

float ReGuiEditor::Noise::perlin2d(float x, float y, int32_t seed) {
	const int cellX = static_cast<int>(std::floor(x));
	const int cellY = static_cast<int>(std::floor(y));
	
	const float localX = x - static_cast<float>(cellX);
	const float localY = y - static_cast<float>(cellY);

	const float blendX = fade(localX);
	const float blendY = fade(localY);

	const float corner00 = getPerlinCornerValue(cellX, cellY, 0, 0, localX, localY, seed);
	const float corner10 = getPerlinCornerValue(cellX, cellY, 1, 0, localX, localY, seed);
	const float corner01 = getPerlinCornerValue(cellX, cellY, 0, 1, localX, localY, seed);
	const float corner11 = getPerlinCornerValue(cellX, cellY, 1, 1, localX, localY, seed);

	const float lerpX0 = lerp(corner00, corner10, blendX);
	const float lerpX1 = lerp(corner01, corner11, blendX);

	return lerp(lerpX0, lerpX1, blendY);
}

float ReGuiEditor::Noise::octavePerlin2d(float x, float y, int octaves, int32_t seed) {
	octaves = std::clamp(octaves, 1, MAX_OCTAVES);

	float sum = 0.0f;
	float amplitudeSum = 0.0f;
	float amplitude = 1.0f;
	float frequency = 1.0f;

	for (int octave = 0; octave < octaves; ++octave) {
		sum += perlin2d(x * frequency, y * frequency, seed + octave) * amplitude;
		
		amplitudeSum += amplitude;

		amplitude *= 0.5f;
		frequency *= 2.0f;
	}

	return sum / amplitudeSum;
}

float ReGuiEditor::Noise::simplexGradient1d(int hash, float x) {
	const float magnitude = 1.0f + static_cast<float>(hash & 7);
	return (hash & 8) == 0 ? magnitude * x : -magnitude * x;
}

float ReGuiEditor::Noise::simplexGradient2d(int hash, float x, float y) {
	hash &= 7;

	const float major = (hash & 4) == 0 ? x : y;
	const float minor = (hash & 4) == 0 ? y : x;

	return ((hash & 1) ? -major : major) + ((hash & 2) ? -2.0f * minor : 2.0f * minor);
}

float ReGuiEditor::Noise::simplexGradient3d(int hash, float x, float y, float z) {
	hash &= 15;
	
	const float a = hash < 8 ? x : y;
	const float b = hash < 4 ? y : (hash == 12 || hash == 14 ? x : z);
	
	return ((hash & 1) ? -a : a) + ((hash & 2) ? -b : b);
}

float ReGuiEditor::Noise::simplexFalloff(float radiusSquared, float distanceSquared) {
	const float t = 0.5f - distanceSquared / radiusSquared;
	if (t < 0.0f)
		return 0.0f;

	return t * t * t * t;
}

float ReGuiEditor::Noise::getSimplexCornerValue(int cell, int offset, float local, int dimension) {
	const float distance = local - static_cast<float>(offset);
	const float distanceSquared = distance * distance;

	switch (dimension) {
	case 1: return simplexFalloff(1.0f, distanceSquared) * simplexGradient1d(PERMUTATION[(static_cast<size_t>(cell) + offset) & 255], distance);
	case 2: return simplexFalloff(0.5f, distanceSquared) * simplexGradient2d(PERMUTATION[(static_cast<size_t>(cell) + offset) & 255], distance, 0.0f);
	case 3: return simplexFalloff(0.6f, distanceSquared) * simplexGradient3d(PERMUTATION[(static_cast<size_t>(cell) + offset) & 255], distance, 0.0f, 0.0f);
	default: return 0.0f;
	}
}

float ReGuiEditor::Noise::simplex1d(float x) {
	const int cell = static_cast<int>(std::floor(x));
	const float local = x - static_cast<float>(cell);

	return 0.395f * (getSimplexCornerValue(cell, 0, local, 1) + getSimplexCornerValue(cell, 1, local, 1));
}

float ReGuiEditor::Noise::simplex2d(float x, float y) {
	constexpr float SKEW           = 0.3660254037844386f;   // (sqrt(3) - 1) / 2
	constexpr float UNSKEW         = 0.21132486540518713f; // (3 - sqrt(3)) / 6
	constexpr float RADIUS_SQUARED = 0.5f;
	constexpr float SCALE          = 45.23065f;

	const float skew = (x + y) * SKEW;
	const int cellX = static_cast<int>(std::floor(x + skew));
	const int cellY = static_cast<int>(std::floor(y + skew));
	
	const float unskew = static_cast<float>(cellX + cellY) * UNSKEW;
	const float localX = x - (static_cast<float>(cellX) - unskew);
	const float localY = y - (static_cast<float>(cellY) - unskew);

	const int stepX = localX > localY ? 1 : 0;
	const int stepY = 1 - stepX;

	const int wrappedX = cellX & 255;
	const int wrappedY = cellY & 255;

	const float corner00 = getSimplexCornerValue(wrappedX, 0,     localX, 2);
	const float corner10 = getSimplexCornerValue(wrappedX, stepX, localX, 2);
	const float corner01 = getSimplexCornerValue(wrappedX, 1,     localX, 2);

	return SCALE * (corner00 + corner10 + corner01);
}

float ReGuiEditor::Noise::simplex3d(float x, float y, float z) {
	constexpr float SKEW           = 1.0f / 3.0f;
	constexpr float UNSKEW         = 1.0f / 6.0f;
	constexpr float RADIUS_SQUARED = 0.6f;
	constexpr float SCALE          = 32.0f;

	const float skew = (x + y + z) * SKEW;
	const int cellX = static_cast<int>(std::floor(x + skew));
	const int cellY = static_cast<int>(std::floor(y + skew));
	const int cellZ = static_cast<int>(std::floor(z + skew));

	const float unskew = static_cast<float>(cellX + cellY + cellZ) * UNSKEW;
	const float localX = x - (static_cast<float>(cellX) - unskew);
	const float localY = y - (static_cast<float>(cellY) - unskew);
	const float localZ = z - (static_cast<float>(cellZ) - unskew);

	struct Step {
		int x;
		int y;
		int z;
	};

	Step first;
	Step second;

	if (localX >= localY) {
		if (localY >= localZ) {
			first =  { 1, 0, 0 };
			second = { 1, 1, 0 };
		} else if (localX >= localZ) {
			first =  { 1, 0, 0 };
			second = { 1, 0, 1 };
		} else {
			first =  { 0, 0, 1 };
			second = { 1, 0, 1 };
		}
	} else {
		if (localY < localZ) {
			first =  { 0, 0, 1 };
			second = { 0, 1, 1 };
		} else if (localX < localZ) {
			first =  { 0, 1, 0 };
			second = { 0, 1, 1 };
		} else {
			first =  { 0, 1, 0 };
			second = { 1, 1, 0 };
		}
	}

	const int wrappedX = cellX & 255;
	const int wrappedY = cellY & 255;
	const int wrappedZ = cellZ & 255;

	const float corner000 = getSimplexCornerValue(wrappedX, 0,        localX, 3);
	const float corner100 = getSimplexCornerValue(wrappedX, first.x , localX, 3);
	const float corner010 = getSimplexCornerValue(wrappedX, second.x, localX, 3);
	const float corner001 = getSimplexCornerValue(wrappedX, 1,        localX, 3);

	return SCALE * (corner000 + corner100 + corner010 + corner001);
}
