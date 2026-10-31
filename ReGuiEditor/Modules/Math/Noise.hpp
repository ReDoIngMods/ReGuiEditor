#pragma once
#include <cstdint>

namespace ReGuiEditor::Noise {
	constexpr int MAX_OCTAVES = 16;

	uint32_t hash2d(int32_t x, int32_t y, int32_t seed);

	float perlinGradient(uint32_t hash, float x, float y);
	float getPerlinCornerValue(int cellX, int cellY, int offsetX, int offsetY, float localX, float localY, int32_t seed);

	float perlin2d(float x, float y, int32_t seed);
	float octavePerlin2d(float x, float y, int octaves, int32_t seed);

	float simplexGradient1d(int hash, float x);
	float simplexGradient2d(int hash, float x, float y);
	float simplexGradient3d(int hash, float x, float y, float z);

	float simplexFalloff(float radiusSquared, float distanceSquared);
	float getSimplexCornerValue(int cell, int offset, float local, int dimension);
	float simplex1d(float x);
	float simplex2d(float x, float y);
	float simplex3d(float x, float y, float z);
}