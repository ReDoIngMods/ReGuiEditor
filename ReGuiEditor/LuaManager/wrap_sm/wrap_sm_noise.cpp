#include "wrap_sm_noise.hpp" // VCIC-Excluded

#include "../UserdataManager.hpp"

#include <Modules/Math/Constants.hpp>
#include <Modules/Math/Easing.hpp>
#include <Modules/Math/Noise.hpp>
#include <Modules/Math/Random.hpp>
#include <Modules/Math/Vector3.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

using Vector3 = ReGuiEditor::Vector3;

static int noise_intNoise2d(lua_State* L) {
	const int cellX = static_cast<int>(std::floor(static_cast<float>(luaL_checknumber(L, 1))));
	const int cellY = static_cast<int>(std::floor(static_cast<float>(luaL_checknumber(L, 2))));
	const int32_t seed = static_cast<int32_t>(luaL_checkinteger(L, 3));

	lua_pushnumber(L, static_cast<lua_Number>(ReGuiEditor::Noise::hash2d(cellX, cellY, seed)));
	return 1;
}

static int noise_floatNoise2d(lua_State* L) {
	const int cellX = static_cast<int>(std::floor(static_cast<float>(luaL_checknumber(L, 1))));
	const int cellY = static_cast<int>(std::floor(static_cast<float>(luaL_checknumber(L, 2))));
	const int32_t seed = static_cast<int32_t>(luaL_checkinteger(L, 3));

	const uint32_t hash = ReGuiEditor::Noise::hash2d(cellX, cellY, seed);
	lua_pushnumber(L, static_cast<lua_Number>(hash >> 8) / 16777216.0);

	return 1;
}

static int noise_perlinNoise2d(lua_State* L) {
	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const int32_t seed = static_cast<int32_t>(luaL_checkinteger(L, 3));

	lua_pushnumber(L, static_cast<lua_Number>(ReGuiEditor::Noise::perlin2d(x, y, seed)));
	return 1;
}

static int noise_octaveNoise2d(lua_State* L) {
	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const int octaves = static_cast<int>(luaL_checkinteger(L, 3));
	const int32_t seed = static_cast<int32_t>(luaL_checkinteger(L, 4));

	luaL_argcheck(L, octaves >= 1 && octaves <= ReGuiEditor::Noise::MAX_OCTAVES, 3, "octaves out of range");
	
	lua_pushnumber(L, static_cast<lua_Number>(ReGuiEditor::Noise::octavePerlin2d(x, y, octaves, seed)));
	return 1;
}

static int noise_simplexNoise1d(lua_State* L) {
	const float x = static_cast<float>(luaL_checknumber(L, 1));

	lua_pushnumber(L, static_cast<lua_Number>(ReGuiEditor::Noise::simplex1d(x)));
	return 1;
}

static int noise_simplexNoise2d(lua_State* L) {
	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));

	lua_pushnumber(L, static_cast<lua_Number>(ReGuiEditor::Noise::simplex2d(x, y)));
	return 1;
}

static int noise_simplexNoise3d(lua_State* L) {
	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const float z = static_cast<float>(luaL_checknumber(L, 3));

	lua_pushnumber(L, static_cast<lua_Number>(ReGuiEditor::Noise::simplex3d(x, y, z)));
	return 1;
}

static int noise_randomRange(lua_State* L) {
	const float min = static_cast<float>(luaL_checknumber(L, 1));
	const float max = static_cast<float>(luaL_checknumber(L, 2));

	lua_pushnumber(L, static_cast<lua_Number>(ReGuiEditor::lerp(min, max, ReGuiEditor::randomGenerateFloat())));
	return 1;
}

static int noise_randomNormalDistribution(lua_State* L) {
	const float mean = static_cast<float>(luaL_checknumber(L, 1));
	const float deviation = static_cast<float>(luaL_checknumber(L, 2));

	lua_pushnumber(L, static_cast<lua_Number>(ReGuiEditor::normalRandomGenerateFloat(mean, deviation)));
	return 1;
}


static int noise_gunSpread(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();

	const Vector3& direction = userdataManager.check<Vector3>(L, 1);
	const float spread = static_cast<float>(luaL_checknumber(L, 2)) * ReGuiEditor::DEGREES_TO_RADIANS_FLOAT;

	const float length = direction.length();
	if (length <= 0.0f) {
		userdataManager.create<Vector3>(L, direction);
		return 1;
	}

	const Vector3 forward = direction / length;
	const Vector3 helper = std::abs(forward.z) < 0.9f ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(1.0f, 0.0f, 0.0f);
	const Vector3 tangent = forward.cross(helper);
	const Vector3 right = tangent / tangent.length();
	const Vector3 up = forward.cross(right);

	const float cosTheta = ReGuiEditor::lerp(1.0f, std::cos(spread), ReGuiEditor::randomGenerateFloat());
	const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
	const float phi = ReGuiEditor::randomGenerateFloat(0.0f, 2.0f * ReGuiEditor::PI_FLOAT);

	const Vector3 offset = (right * std::cos(phi) + up * std::sin(phi)) * sinTheta;

	userdataManager.create<Vector3>(L, (forward * cosTheta + offset) * length);
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