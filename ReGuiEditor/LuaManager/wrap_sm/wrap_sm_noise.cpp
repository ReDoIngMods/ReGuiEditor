#include "wrap_sm_noise.hpp" // VCIC-Excluded

#include "../UserdataManager.hpp"

#include <UTILS/Vector3.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <random>

// NOTE: The game's exact hash/noise algorithms are unknown. Outputs are deterministic, but won't match the game's values.

constexpr float PI = 3.14159265358979323846f;
constexpr float DEGREES_TO_RADIANS = PI / 180.0f;
constexpr int MAX_OCTAVES = 32;

static thread_local std::random_device randomDevice;
static thread_local std::mt19937 randomGenerator(randomDevice());

inline static float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

inline static float randomUnitFloat() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(randomGenerator); }

inline static constexpr uint32_t finalize(uint32_t hash) {
	hash ^= hash >> 16;
	hash *= 0x85EBCA6Bu;
	hash ^= hash >> 13;
	hash *= 0xC2B2AE35u;
	hash ^= hash >> 16;
	return hash;
}

inline static constexpr uint32_t hash2d(int32_t x, int32_t y, int32_t seed) {
	const uint32_t hashedSeed = finalize(static_cast<uint32_t>(seed));
	const uint32_t hashedY = finalize(static_cast<uint32_t>(y) * 0x85EBCA77u ^ hashedSeed);

	return finalize(static_cast<uint32_t>(x) * 0x9E3779B1u ^ hashedY);
}

inline static float perlinGradient(uint32_t hash, float x, float y) {
	switch (hash & 3) {
	case 0:  return  x + y;
	case 1:  return -x + y;
	case 2:  return  x - y;
	default: return -x - y;
	}
}

inline static float getPerlinCornerValue(int cellX, int cellY, int offsetX, int offsetY, float localX, float localY, int32_t seed) {
	const uint32_t hash = hash2d(cellX + offsetX, cellY + offsetY, seed);
	return perlinGradient(hash, localX - static_cast<float>(offsetX), localY - static_cast<float>(offsetY));
}

inline static float perlin2d(float x, float y, int32_t seed) {
	const int cellX = static_cast<int>(std::floor(x));
	const int cellY = static_cast<int>(std::floor(y));

	const float localX = x - static_cast<float>(cellX);
	const float localY = y - static_cast<float>(cellY);

	const float blendX = fade(localX);
	const float blendY = fade(localY);

	const float bottom = lerp(getPerlinCornerValue(cellX, cellY, 0, 0, localX, localY, seed), getPerlinCornerValue(cellX, cellY, 1, 0, localX, localY, seed), blendX);
	const float top = lerp(getPerlinCornerValue(cellX, cellY, 0, 1, localX, localY, seed), getPerlinCornerValue(cellX, cellY, 1, 1, localX, localY, seed), blendX);

	return lerp(bottom, top, blendY);
}

inline static float octavePerlin2d(float x, float y, int octaves, int32_t seed) {
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

inline static std::array<uint8_t, 512> generatePermutationTable() {
	std::array<uint8_t, 256> shuffled{};
	for (size_t index = 0; index < shuffled.size(); ++index)
		shuffled[index] = static_cast<uint8_t>(index);

	std::shuffle(shuffled.begin(), shuffled.end(), randomGenerator);

	std::array<uint8_t, 512> table{};
	for (size_t index = 0; index < table.size(); ++index)
		table[index] = shuffled[index & 255];

	return table;
}

static thread_local const std::array<uint8_t, 512> PERMUTATION = generatePermutationTable();

inline static float simplexGradient1d(int hash, float x) {
	const float magnitude = 1.0f + static_cast<float>(hash & 7);
	return ((hash & 8) ? -magnitude : magnitude) * x;
}

inline static float simplexGradient2d(int hash, float x, float y) {
	hash &= 7;

	const float major = hash < 4 ? x : y;
	const float minor = hash < 4 ? y : x;

	return ((hash & 1) ? -major : major) + ((hash & 2) ? -2.0f * minor : 2.0f * minor);
}

inline static float simplexGradient3d(int hash, float x, float y, float z) {
	hash &= 15;

	const float a = hash < 8 ? x : y;
	const float b = hash < 4 ? y : (hash == 12 || hash == 14 ? x : z);

	return ((hash & 1) ? -a : a) + ((hash & 2) ? -b : b);
}

inline static float simplexFalloff(float radiusSquared, float distanceSquared) {
	const float t = radiusSquared - distanceSquared;
	if (t < 0.0f)
		return 0.0f;

	return t * t * t * t;
}

inline static float getSimplexCornerValue(int cell, int offset, float local, int dimension) {
	const float distance = local - static_cast<float>(offset);
	const float distanceSquared = distance * distance;
	
	switch (dimension) {
	case 1: return simplexFalloff(1.0f, distanceSquared) * simplexGradient1d(PERMUTATION[(static_cast<size_t>(cell) + offset) & 255], distance);
	case 2: return simplexFalloff(0.5f, distanceSquared) * simplexGradient2d(PERMUTATION[(static_cast<size_t>(cell) + offset) & 255], distance, 0.0f);
	case 3: return simplexFalloff(0.6f, distanceSquared) * simplexGradient3d(PERMUTATION[(static_cast<size_t>(cell) + offset) & 255], distance, 0.0f, 0.0f);
	default: return 0.0f;
	}
}

inline static float simplex1d(float x) {
	const int cell = static_cast<int>(std::floor(x));
	const float local = x - static_cast<float>(cell);

	return 0.395f * (getSimplexCornerValue(cell, 0, local, 1) + getSimplexCornerValue(cell, 1, local, 1));
}

inline static float simplex2d(float x, float y) {
	constexpr float SKEW = 0.3660254037844386f;   // (sqrt(3) - 1) / 2
	constexpr float UNSKEW = 0.21132486540518713f; // (3 - sqrt(3)) / 6
	constexpr float RADIUS_SQUARED = 0.5f;
	constexpr float SCALE = 45.23065f;

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

	return SCALE * (getSimplexCornerValue(wrappedX, 0, localX, 2) + getSimplexCornerValue(wrappedX, stepX, localX, 2) + getSimplexCornerValue(wrappedX, 1, localX, 2));
}

inline static float simplex3d(float x, float y, float z) {
	constexpr float SKEW = 1.0f / 3.0f;
	constexpr float UNSKEW = 1.0f / 6.0f;
	constexpr float RADIUS_SQUARED = 0.6f;
	constexpr float SCALE = 32.0f;

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
	Step first, second;

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

	return SCALE * (getSimplexCornerValue(wrappedX, 0, localX, 3) + getSimplexCornerValue(wrappedX, first.x, localX, 3) + getSimplexCornerValue(wrappedX, second.x, localX, 3) + getSimplexCornerValue(wrappedX, 1, localX, 3));
}

static int noise_intNoise2d(lua_State* L) {
	const int cellX = static_cast<int>(std::floor(static_cast<float>(luaL_checknumber(L, 1))));
	const int cellY = static_cast<int>(std::floor(static_cast<float>(luaL_checknumber(L, 2))));
	const int32_t seed = static_cast<int32_t>(luaL_checkinteger(L, 3));

	lua_pushnumber(L, static_cast<lua_Number>(hash2d(cellX, cellY, seed)));
	return 1;
}

static int noise_floatNoise2d(lua_State* L) {
	const int cellX = static_cast<int>(std::floor(static_cast<float>(luaL_checknumber(L, 1))));
	const int cellY = static_cast<int>(std::floor(static_cast<float>(luaL_checknumber(L, 2))));
	const int32_t seed = static_cast<int32_t>(luaL_checkinteger(L, 3));

	const uint32_t hash = hash2d(cellX, cellY, seed);
	lua_pushnumber(L, static_cast<lua_Number>(hash >> 8) / 16777216.0);

	return 1;
}

static int noise_perlinNoise2d(lua_State* L) {
	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const int32_t seed = static_cast<int32_t>(luaL_checkinteger(L, 3));

	lua_pushnumber(L, static_cast<lua_Number>(perlin2d(x, y, seed)));
	return 1;
}

static int noise_octaveNoise2d(lua_State* L) {
	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const int octaves = static_cast<int>(luaL_checkinteger(L, 3));
	const int32_t seed = static_cast<int32_t>(luaL_checkinteger(L, 4));

	luaL_argcheck(L, octaves >= 1 && octaves <= MAX_OCTAVES, 3, "octaves out of range");
	
	lua_pushnumber(L, static_cast<lua_Number>(octavePerlin2d(x, y, octaves, seed)));
	return 1;
}

static int noise_simplexNoise1d(lua_State* L) {
	const float x = static_cast<float>(luaL_checknumber(L, 1));

	lua_pushnumber(L, static_cast<lua_Number>(simplex1d(x)));
	return 1;
}

static int noise_simplexNoise2d(lua_State* L) {
	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));

	lua_pushnumber(L, static_cast<lua_Number>(simplex2d(x, y)));
	return 1;
}

static int noise_simplexNoise3d(lua_State* L) {
	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const float z = static_cast<float>(luaL_checknumber(L, 3));

	lua_pushnumber(L, static_cast<lua_Number>(simplex3d(x, y, z)));
	return 1;
}

static int noise_randomRange(lua_State* L) {
	const float min = static_cast<float>(luaL_checknumber(L, 1));
	const float max = static_cast<float>(luaL_checknumber(L, 2));

	lua_pushnumber(L, static_cast<lua_Number>(lerp(min, max, randomUnitFloat())));
	return 1;
}

static int noise_randomNormalDistribution(lua_State* L) {
	const float mean = static_cast<float>(luaL_checknumber(L, 1));
	const float deviation = static_cast<float>(luaL_checknumber(L, 2));

	lua_pushnumber(L, static_cast<lua_Number>(std::normal_distribution<float>(mean, deviation)(randomGenerator)));
	return 1;
}


static int noise_gunSpread(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();

	const UTILS::Vector3& direction = userdataManager.check<UTILS::Vector3>(L, 1);
	const float spread = static_cast<float>(luaL_checknumber(L, 2)) * DEGREES_TO_RADIANS;

	const float length = direction.length();
	if (length <= 0.0f) {
		userdataManager.create<UTILS::Vector3>(L, direction);
		return 1;
	}

	const UTILS::Vector3 forward = direction / length;
	const UTILS::Vector3 helper = std::abs(forward.z) < 0.9f ? UTILS::Vector3(0.0f, 0.0f, 1.0f) : UTILS::Vector3(1.0f, 0.0f, 0.0f);
	const UTILS::Vector3 tangent = forward.cross(helper);
	const UTILS::Vector3 right = tangent / tangent.length();
	const UTILS::Vector3 up = forward.cross(right);

	const float cosTheta = lerp(1.0f, std::cos(spread), randomUnitFloat());
	const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
	const float phi = randomUnitFloat() * 2.0f * PI;

	const UTILS::Vector3 offset = (right * std::cos(phi) + up * std::sin(phi)) * sinTheta;

	userdataManager.create<UTILS::Vector3>(L, (forward * cosTheta + offset) * length);
	return 1;
}

void wrap_sm_noise(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.noise' library...");

	static const luaL_Reg functions[] = {
		{"floatNoise2d", noise_floatNoise2d},
		{"gunSpread", noise_gunSpread},
		{"intNoise2d", noise_intNoise2d},
		{"octaveNoise2d", noise_octaveNoise2d},
		{"perlinNoise2d", noise_perlinNoise2d},
		{"randomNormalDistribution", noise_randomNormalDistribution},
		{"randomRange", noise_randomRange},
		{"simplexNoise1d", noise_simplexNoise1d},
		{"simplexNoise2d", noise_simplexNoise2d},
		{"simplexNoise3d", noise_simplexNoise3d},

		{nullptr, nullptr}
	};

	lua_newtable(L);
	luaL_register(L, nullptr, functions);
	lua_setfield(L, -2, "noise");
}