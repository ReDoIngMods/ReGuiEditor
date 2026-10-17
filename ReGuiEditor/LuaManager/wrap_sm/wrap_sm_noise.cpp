#include "wrap_sm_noise.hpp" // VCIC-Excluded

#include "../UserdataManager.hpp"

#include <UTILS/Hashing.hpp>
#include <UTILS/Vector3.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <random>

// UGLY WARNING! I REPEAT! UGLY FUCKING CODE WARNING! SPARE YO FUCKING EYES .-.

static thread_local std::random_device randomDevice;
static thread_local std::mt19937 randomGenerator(randomDevice());

constexpr float PI = 3.14159265358979323846f;

inline static float getRandomFloat() {
	return std::uniform_real_distribution<float>(0.0f, 1.0f)(randomGenerator);
}

inline static uint32_t hash(int32_t x, int32_t y, int32_t z, int32_t seed) {
	size_t hash = 0;
	UTILS::hashCombine(hash, x);
	UTILS::hashCombine(hash, y);
	UTILS::hashCombine(hash, z);
	UTILS::hashCombine(hash, seed);

	return static_cast<uint32_t>(hash);
}

inline static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
inline static float mix(float a, float b, float t) { return a + (b - a) * t; }

inline static float gradient1(uint32_t h, float x) { return (h & 1) ? x : -x; }

inline static float gradient2(uint32_t h, float x, float y) {
	switch (h & 3) {
	case 0:  return x + y;
	case 1:  return -x + y;
	case 2:  return x - y;
	default: return -x - y;
	}
}

inline static float gradient3(uint32_t h, float x, float y, float z) {
	switch (h % 12) {
	case 0:  return x + y;
	case 1:  return -x + y;
	case 2:  return x - y;
	case 3:  return -x - y;
	case 4:  return x + z;
	case 5:  return -x + z;
	case 6:  return x - z;
	case 7:  return -x - z;
	case 8:  return y + z;
	case 9:  return -y + z;
	case 10: return y - z;
	default: return -y - z;
	}
}

inline static float perlin1(float x, int32_t seed) {
	const int32_t x0 = static_cast<int32_t>(std::floor(x));
	const float fx = x - x0;

	return std::clamp(mix(gradient1(hash(x0, 0, 0, seed), fx), gradient1(hash(x0 + 1, 0, 0, seed), fx - 1.0f), fade(fx)) * 2.0f, -1.0f, 1.0f);
}

inline static float perlin2(float x, float y, int32_t seed) {
	const int32_t x0 = static_cast<int32_t>(std::floor(x)), y0 = static_cast<int32_t>(std::floor(y));
	const float fx = x - x0, fy = y - y0;
	const float u = fade(fx), v = fade(fy);

	const float bottom = mix(gradient2(hash(x0, y0, 0, seed), fx, fy), gradient2(hash(x0 + 1, y0, 0, seed), fx - 1.0f, fy), u);
	const float top = mix(gradient2(hash(x0, y0 + 1, 0, seed), fx, fy - 1.0f), gradient2(hash(x0 + 1, y0 + 1, 0, seed), fx - 1.0f, fy - 1.0f), u);

	return std::clamp(mix(bottom, top, v), -1.0f, 1.0f);
}

inline static float getGradientCorner(int32_t x, int32_t y, int32_t z, float fx, float fy, float fz, int32_t seed) {
	return gradient3(hash(x, y, z, seed), fx, fy, fz);
}

inline static float perlin3(float x, float y, float z, int32_t seed) {
	const int32_t x0 = static_cast<int32_t>(std::floor(x)), y0 = static_cast<int32_t>(std::floor(y)), z0 = static_cast<int32_t>(std::floor(z));
	const float fx = x - x0, fy = y - y0, fz = z - z0;
	const float u = fade(fx), v = fade(fy), w = fade(fz);

	const float bottom = mix(mix(getGradientCorner(x0, y0, z0, fx, fy, fz, seed), getGradientCorner(x0 + 1, y0, z0, fx - 1.0f, fy, fz, seed), u), mix(getGradientCorner(x0, y0 + 1, z0, fx, fy - 1.0f, fz, seed), getGradientCorner(x0 + 1, y0 + 1, z0, fx - 1.0f, fy - 1.0f, fz, seed), u), v);
	const float top = mix(mix(getGradientCorner(x0, y0, z0 + 1, fx, fy, fz - 1.0f, seed), getGradientCorner(x0 + 1, y0, z0 + 1, fx - 1.0f, fy, fz - 1.0f, seed), u), mix(getGradientCorner(x0, y0 + 1, z0 + 1, fx, fy - 1.0f, fz - 1.0f, seed), getGradientCorner(x0 + 1, y0 + 1, z0 + 1, fx - 1.0f, fy - 1.0f, fz - 1.0f, seed), u), v);

	return std::clamp(mix(bottom, top, w), -1.0f, 1.0f);
}

template <typename Sample>
inline static float fractal(lua_State* L, int firstOptionalIndex, Sample sample) {
	const int octaves = static_cast<int>(luaL_optinteger(L, firstOptionalIndex, 1));
	const float persistence = static_cast<float>(luaL_optnumber(L, firstOptionalIndex + 1, 0.5));
	const int32_t seed = static_cast<int32_t>(luaL_optinteger(L, firstOptionalIndex + 2, 0));

	if (octaves < 1 || octaves > 16)
		luaL_error(L, "octaves must be between 1 and 16");

	float sum = 0.0f, amplitude = 1.0f, total = 0.0f, frequency = 1.0f;
	for (int octave = 0; octave < octaves; ++octave) {
		sum += sample(frequency, seed + octave) * amplitude;
		total += amplitude;

		amplitude *= persistence;
		frequency *= 2.0f;
	}

	return sum / total;
}

inline static float checkFloat(lua_State* L, int index) {
	return static_cast<float>(luaL_checknumber(L, index));
}

// API

static int noise_randomRange(lua_State* L) {
	const float minimum = checkFloat(L, 1);
	const float maximum = checkFloat(L, 2);

	lua_pushnumber(L, minimum + (maximum - minimum) * getRandomFloat());
	return 1;
}

static int noise_intNoise2d(lua_State* L) {
	const uint32_t value = hash(static_cast<int32_t>(luaL_checkinteger(L, 1)), static_cast<int32_t>(luaL_checkinteger(L, 2)), 0, static_cast<int32_t>(luaL_optinteger(L, 3, 0)));

	lua_pushinteger(L, static_cast<lua_Integer>(value));
	return 1;
}

static int noise_floatNoise2d(lua_State* L) {
	const uint32_t value = hash(static_cast<int32_t>(luaL_checkinteger(L, 1)), static_cast<int32_t>(luaL_checkinteger(L, 2)), 0, static_cast<int32_t>(luaL_optinteger(L, 3, 0)));

	lua_pushnumber(L, (value >> 8) / 16777216.0);
	return 1;
}

static int noise_perlin1d(lua_State* L) {
	const float x = checkFloat(L, 1);

	lua_pushnumber(L, fractal(L, 2, [x](float frequency, int32_t seed) { return perlin1(x * frequency, seed); }));
	return 1;
}

static int noise_perlin2d(lua_State* L) {
	const float x = checkFloat(L, 1), y = checkFloat(L, 2);

	lua_pushnumber(L, fractal(L, 3, [x, y](float frequency, int32_t seed) { return perlin2(x * frequency, y * frequency, seed); }));
	return 1;
}

static int noise_perlin3d(lua_State* L) {
	const float x = checkFloat(L, 1), y = checkFloat(L, 2), z = checkFloat(L, 3);

	lua_pushnumber(L, fractal(L, 4, [x, y, z](float frequency, int32_t seed) { return perlin3(x * frequency, y * frequency, z * frequency, seed); }));
	return 1;
}

static int noise_gunSpread(lua_State* L) {
	const UTILS::Vector3& direction = UserdataManager::getInstance().check<UTILS::Vector3>(L, 1);
	const float spread = checkFloat(L, 2) * (PI / 180.0f);

	const float length = direction.length();
	if (length <= 0.0f) {
		UserdataManager::getInstance().create<UTILS::Vector3>(L, direction);
		return 1;
	}

	const UTILS::Vector3 forward = direction / length;

	const float cosTheta = 1.0f - getRandomFloat() * (1.0f - std::cos(spread));
	const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
	const float phi = getRandomFloat() * 2.0f * PI;

	const UTILS::Vector3 helper = std::abs(forward.z) < 0.9f ? UTILS::Vector3(0.0f, 0.0f, 1.0f) : UTILS::Vector3(1.0f, 0.0f, 0.0f);
	const UTILS::Vector3 right = forward.cross(helper) / forward.cross(helper).length();
	const UTILS::Vector3 up = forward.cross(right);

	UserdataManager::getInstance().create<UTILS::Vector3>(L, (forward * cosTheta + (right * std::cos(phi) + up * std::sin(phi)) * sinTheta) * length);
	return 1;
}

void wrap_sm_noise(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.noise' library...");

	static const luaL_Reg functions[] = {
		{"randomRange", noise_randomRange},
		{"intNoise2d", noise_intNoise2d},
		{"floatNoise2d", noise_floatNoise2d},
		{"perlin1d", noise_perlin1d},
		{"perlin2d", noise_perlin2d},
		{"perlin3d", noise_perlin3d},
		{"gunSpread", noise_gunSpread},

		{nullptr, nullptr}
	};

	lua_newtable(L);
	luaL_register(L, nullptr, functions);
	lua_setfield(L, -2, "noise");
}
