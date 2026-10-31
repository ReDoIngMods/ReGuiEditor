#include "wrap_sm_util.hpp" // VCIC-Excluded

#include "../UserdataManager.hpp"

#include <Modules/Math/Easing.hpp>
#include <Modules/Math/Quaternion.hpp>
#include <Modules/Math/Vector3.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using Vector3 = ReGuiEditor::Vector3;
using Quaternion = ReGuiEditor::Quaternion;

constexpr double PI = 3.14159265358979323846;

static int util_clamp(lua_State* L) {
	const double value = luaL_checknumber(L, 1);
	const double minimum = luaL_checknumber(L, 2);
	const double maximum = luaL_checknumber(L, 3);

	lua_pushnumber(L, std::min(std::max(value, minimum), maximum));
	return 1;
}

static int util_lerp(lua_State* L) {
	const double a = luaL_checknumber(L, 1);
	const double b = luaL_checknumber(L, 2);
	const double t = luaL_checknumber(L, 3);

	lua_pushnumber(L, a + (b - a) * t);
	return 1;
}

static int util_positiveModulo(lua_State* L) {
	const double x = luaL_checknumber(L, 1);
	const double n = luaL_checknumber(L, 2);

	lua_pushnumber(L, x - std::floor(x / n) * n);
	return 1;
}

static int util_smoothstep(lua_State* L) {
	const double edge0 = luaL_checknumber(L, 1);
	const double edge1 = luaL_checknumber(L, 2);
	const double t = std::clamp((luaL_checknumber(L, 3) - edge0) / (edge1 - edge0), 0.0, 1.0);

	lua_pushnumber(L, t * t * (3.0 - 2.0 * t));
	return 1;
}

static int util_smootherstep(lua_State* L) {
	const double edge0 = luaL_checknumber(L, 1);
	const double edge1 = luaL_checknumber(L, 2);
	const double t = std::clamp((luaL_checknumber(L, 3) - edge0) / (edge1 - edge0), 0.0, 1.0);

	lua_pushnumber(L, t * t * t * (t * (t * 6.0 - 15.0) + 10.0));
	return 1;
}

static int util_easing(lua_State* L) {
	const double t = luaL_checknumber(L, 2);
	if (lua_type(L, 1) == LUA_TNUMBER) {
		const lua_Integer id = lua_tointeger(L, 1);
		if (!ReGuiEditor::easingTypeidValid(static_cast<int>(id)))
			return luaL_argerror(L, 1, "invalid easing id");

		lua_pushnumber(L, ReGuiEditor::easingCalculate(t, static_cast<ReGuiEditor::EasingType>(id)));
		return 1;
	}

	if (lua_type(L, 1) == LUA_TSTRING) {
		const char* name = lua_tostring(L, 1);
		if (!ReGuiEditor::easingTypenameValid(name))
			return luaL_argerror(L, 1, "invalid easing name");

		lua_pushnumber(L, ReGuiEditor::easingCalculate(t, name));
		return 1;
	}

	return luaL_argerror(L, 1, "expected easing id (number) or easing name (string)");
}

static int util_bezier2(lua_State* L) {
	const double c0 = luaL_checknumber(L, 1);
	const double c1 = luaL_checknumber(L, 2);
	const double c2 = luaL_checknumber(L, 3);
	const double t = luaL_checknumber(L, 4);

	const double u = 1.0 - t;

	lua_pushnumber(L, u * u * c0 + 2.0 * u * t * c1 + t * t * c2);
	return 1;
}

static int util_bezier3(lua_State* L) {
	const double c0 = luaL_checknumber(L, 1);
	const double c1 = luaL_checknumber(L, 2);
	const double c2 = luaL_checknumber(L, 3);
	const double c3 = luaL_checknumber(L, 4);
	const double t = luaL_checknumber(L, 5);

	const double u = 1.0 - t;

	lua_pushnumber(L, u * u * u * c0 + 3.0 * u * u * t * c1 + 3.0 * u * t * t * c2 + t * t * t * c3);
	return 1;
}

static int util_axesToQuat(lua_State* L) {
	UserdataManager& userdata = UserdataManager::getInstance();

	const Vector3 xAxis = userdata.check<Vector3>(L, 1).normalize();
	const Vector3 zAxis = xAxis.cross(userdata.check<Vector3>(L, 2)).normalize();
	const Vector3 yAxis = zAxis.cross(xAxis);

	const float m00 = xAxis.x, m10 = xAxis.y, m20 = xAxis.z;
	const float m01 = yAxis.x, m11 = yAxis.y, m21 = yAxis.z;
	const float m02 = zAxis.x, m12 = zAxis.y, m22 = zAxis.z;

	float x, y, z, w;
	const float trace = m00 + m11 + m22;

	if (trace > 0.0f) {
		const float s = std::sqrt(trace + 1.0f) * 2.0f;
		w = 0.25f * s; x = (m21 - m12) / s; y = (m02 - m20) / s; z = (m10 - m01) / s;
	} else if (m00 > m11 && m00 > m22) {
		const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
		w = (m21 - m12) / s; x = 0.25f * s; y = (m01 + m10) / s; z = (m02 + m20) / s;
	} else if (m11 > m22) {
		const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
		w = (m02 - m20) / s; x = (m01 + m10) / s; y = 0.25f * s; z = (m12 + m21) / s;
	} else {
		const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
		w = (m10 - m01) / s; x = (m02 + m20) / s; y = (m12 + m21) / s; z = 0.25f * s;
	}

	userdata.create<Quaternion>(L, Quaternion{ x, y, z, w });
	return 1;
}

static int util_geometricMedian(lua_State* L) {
	UserdataManager& userdata = UserdataManager::getInstance();

	luaL_checktype(L, 1, LUA_TTABLE);
	const int iterations = static_cast<int>(luaL_optinteger(L, 3, 20));

	std::vector<Vector3> points;
	points.reserve(lua_objlen(L, 1));

	for (int index = 1;; ++index) {
		lua_rawgeti(L, 1, index);

		const bool isNil = lua_isnil(L, -1);
		if (!isNil)
			points.push_back(userdata.check<Vector3>(L, -1));

		lua_pop(L, 1);

		if (isNil)
			break;
	}

	luaL_argcheck(L, !points.empty(), 1, "expected at least one point");

	Vector3 guess = { 0.0f, 0.0f, 0.0f };

	if (userdata.test<Vector3>(L, 2)) {
		guess = userdata.check<Vector3>(L, 2);
	} else {
		for (const Vector3& point : points)
			guess = guess + point;

		guess = guess / static_cast<float>(points.size());
	}

	for (int iteration = 0; iteration < iterations; ++iteration) {
		Vector3 weighted = { 0.0f, 0.0f, 0.0f };
		float totalWeight = 0.0f;

		for (const Vector3& point : points) {
			const float distance = (point - guess).length();
			if (distance < 1e-6f)
				continue;

			const float weight = 1.0f / distance;
			weighted = weighted + point * weight;
			totalWeight += weight;
		}

		if (totalWeight <= 0.0f)
			break;

		guess = weighted / totalWeight;
	}

	userdata.create<Vector3>(L, guess);
	return 1;
}

static int util_hash(lua_State* L) {
	uint32_t hash = 2166136261u;

	const auto feed = [&hash](const void* data, size_t length) {
		const unsigned char* bytes = static_cast<const unsigned char*>(data);

		for (size_t index = 0; index < length; ++index)
			hash = (hash ^ bytes[index]) * 16777619u;
	};

	for (int index = 1, top = lua_gettop(L); index <= top; ++index) {
		const int type = lua_type(L, index);
		feed(&type, sizeof(type));

		switch (type) {
		case LUA_TNIL:
			break;
		case LUA_TBOOLEAN: {
			const bool value = lua_toboolean(L, index) != 0;
			feed(&value, sizeof(value));
			break;
		}
		case LUA_TNUMBER: {
			const double value = lua_tonumber(L, index) + 0.0; // Normalizes -0
			feed(&value, sizeof(value));
			break;
		}
		case LUA_TSTRING: {
			size_t length = 0;
			const char* value = lua_tolstring(L, index, &length);
			feed(value, length);
			break;
		}
		default:
			return luaL_argerror(L, index, "cannot hash this type");
		}
	}

	lua_pushnumber(L, static_cast<lua_Number>(hash));
	return 1;
}

inline static size_t countCodePoints(const char* text, size_t length) {
	size_t count = 0;

	for (size_t index = 0; index < length; ++index)
		count += (static_cast<unsigned char>(text[index]) & 0xC0) != 0x80;

	return count;
}

static int util_utf8StringLength(lua_State* L) {
	size_t length = 0;
	const char* text = luaL_checklstring(L, 1, &length);

	lua_pushinteger(L, static_cast<lua_Integer>(countCodePoints(text, length)));
	return 1;
}

static int util_clampUtf8String(lua_State* L) {
	size_t length = 0;
	const char* text = luaL_checklstring(L, 1, &length);
	const lua_Integer maxLength = luaL_checkinteger(L, 2);

	size_t end = 0;
	size_t count = 0;
	while (end < length) {
		size_t next = end + 1;
		while (next < length && (static_cast<unsigned char>(text[next]) & 0xC0) == 0x80)
			++next;

		if (static_cast<lua_Integer>(count) >= maxLength)
			break;

		++count;
		end = next;
	}

	lua_pushlstring(L, text, end);
	return 1;
}

void wrap_sm_util(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.util' library...");

	static const luaL_Reg functions[] = {
		{"axesToQuat", util_axesToQuat},
		{"bezier2", util_bezier2},
		{"bezier3", util_bezier3},
		{"clamp", util_clamp},
		{"clampUtf8String", util_clampUtf8String},
		{"easing", util_easing},
		{"geometricMedian", util_geometricMedian},
		{"hash", util_hash},
		{"lerp", util_lerp},
		{"positiveModulo", util_positiveModulo},
		{"smootherstep", util_smootherstep},
		{"smoothstep", util_smoothstep},
		{"utf8StringLength", util_utf8StringLength},

		{nullptr, nullptr}
	};

	lua_newtable(L);
	luaL_register(L, nullptr, functions);
	lua_setfield(L, -2, "util");
}
