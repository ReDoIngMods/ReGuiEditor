#include "wrap_sm_util.hpp" // VCIC-Excluded

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <cmath>
#include <cstring>

constexpr double PI = 3.14159265358979323846;

inline static double checkNumber(lua_State* L, int index) {
	return luaL_checknumber(L, index);
}

template <int N> inline static double easeInPow(double t) { return std::pow(t, N); }
template <int N> inline static double easeOutPow(double t) { return 1.0 - std::pow(1.0 - t, N); }
template <int N> inline static double easeInOutPow(double t) {
	return t < 0.5 ? std::pow(2.0, N - 1) * std::pow(t, N) : 1.0 - std::pow(-2.0 * t + 2.0, N) / 2.0;
}

inline static double bounceOut(double t) {
	constexpr double n1 = 7.5625, d1 = 2.75;
	if (t < 1.0 / d1)
		return n1 * t * t;

	if (t < 2.0 / d1) {
		const double u = t - 1.5 / d1;
		return n1 * u * u + 0.75;
	}

	if (t < 2.5 / d1) {
		const double u = t - 2.25 / d1;
		return n1 * u * u + 0.9375;
	}

	const double u = t - 2.625 / d1;
	return n1 * u * u + 0.984375;
}

constexpr double BACK_C1 = 1.70158;
constexpr double BACK_C2 = BACK_C1 * 1.525;
constexpr double BACK_C3 = BACK_C1 + 1.0;
constexpr double ELASTIC_C4 = (2.0 * PI) / 3.0;
constexpr double ELASTIC_C5 = (2.0 * PI) / 4.5;

struct Easing {
	const char* name;
	double (*function)(double);
};

static const Easing EASINGS[] = {
	{"linear", [](double t) { return t; }},

	{"easeInSine", [](double t) { return 1.0 - std::cos(t * PI / 2.0); }},
	{"easeOutSine", [](double t) { return std::sin(t * PI / 2.0); }},
	{"easeInOutSine", [](double t) { return -(std::cos(PI * t) - 1.0) / 2.0; }},

	{"easeInQuad", easeInPow<2>},
	{"easeOutQuad", easeOutPow<2>},
	{"easeInOutQuad", easeInOutPow<2>},

	{"easeInCubic", easeInPow<3>},
	{"easeOutCubic", easeOutPow<3>},
	{"easeInOutCubic", easeInOutPow<3>},

	{"easeInQuart", easeInPow<4>},
	{"easeOutQuart", easeOutPow<4>},
	{"easeInOutQuart", easeInOutPow<4>},

	{"easeInQuint", easeInPow<5>},
	{"easeOutQuint", easeOutPow<5>},
	{"easeInOutQuint", easeInOutPow<5>},

	{"easeInExpo", [](double t) { return t == 0.0 ? 0.0 : std::pow(2.0, 10.0 * t - 10.0); }},
	{"easeOutExpo", [](double t) { return t == 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * t); }},
	{"easeInOutExpo", [](double t) {
		if (t == 0.0 || t == 1.0)
			return t;

		return t < 0.5 ? std::pow(2.0, 20.0 * t - 10.0) / 2.0 : (2.0 - std::pow(2.0, -20.0 * t + 10.0)) / 2.0;
	}},

	{"easeInCirc", [](double t) { return 1.0 - std::sqrt(1.0 - t * t); }},
	{"easeOutCirc", [](double t) { return std::sqrt(1.0 - (t - 1.0) * (t - 1.0)); }},
	{"easeInOutCirc", [](double t) {
		return t < 0.5
			? (1.0 - std::sqrt(1.0 - 4.0 * t * t)) / 2.0
			: (std::sqrt(1.0 - (-2.0 * t + 2.0) * (-2.0 * t + 2.0)) + 1.0) / 2.0;
	}},

	{"easeInBack", [](double t) { return BACK_C3 * t * t * t - BACK_C1 * t * t; }},
	{"easeOutBack", [](double t) { return 1.0 + BACK_C3 * std::pow(t - 1.0, 3.0) + BACK_C1 * std::pow(t - 1.0, 2.0); }},
	{"easeInOutBack", [](double t) {
		return t < 0.5
			? (std::pow(2.0 * t, 2.0) * ((BACK_C2 + 1.0) * 2.0 * t - BACK_C2)) / 2.0
			: (std::pow(2.0 * t - 2.0, 2.0) * ((BACK_C2 + 1.0) * (t * 2.0 - 2.0) + BACK_C2) + 2.0) / 2.0;
	}},

	{"easeInElastic", [](double t) {
		if (t == 0.0 || t == 1.0)
			return t;

		return -std::pow(2.0, 10.0 * t - 10.0) * std::sin((t * 10.0 - 10.75) * ELASTIC_C4);
	}},

	{"easeOutElastic", [](double t) {
		if (t == 0.0 || t == 1.0)
			return t;

		return std::pow(2.0, -10.0 * t) * std::sin((t * 10.0 - 0.75) * ELASTIC_C4) + 1.0;
	}},

	{"easeInOutElastic", [](double t) {
		if (t == 0.0 || t == 1.0)
			return t;

		return t < 0.5
			? -(std::pow(2.0, 20.0 * t - 10.0) * std::sin((20.0 * t - 11.125) * ELASTIC_C5)) / 2.0
			: (std::pow(2.0, -20.0 * t + 10.0) * std::sin((20.0 * t - 11.125) * ELASTIC_C5)) / 2.0 + 1.0;
	}},

	{"easeInBounce", [](double t) { return 1.0 - bounceOut(1.0 - t); }},
	{"easeOutBounce", bounceOut},
	{"easeInOutBounce", [](double t) {
		return t < 0.5 ? (1.0 - bounceOut(1.0 - 2.0 * t)) / 2.0 : (1.0 + bounceOut(2.0 * t - 1.0)) / 2.0;
	}},
};

static int util_clamp(lua_State* L) {
	const double minimum = checkNumber(L, 2);
	const double maximum = checkNumber(L, 3);
	if (minimum > maximum)
		return luaL_error(L, "sm.util.clamp: min is greater than max");

	lua_pushnumber(L, std::clamp(checkNumber(L, 1), minimum, maximum));
	return 1;
}

static int util_lerp(lua_State* L) {
	const double a = checkNumber(L, 1);
	const double b = checkNumber(L, 2);

	lua_pushnumber(L, a + (b - a) * checkNumber(L, 3));
	return 1;
}

static int util_positiveModulo(lua_State* L) {
	const double x = checkNumber(L, 1);
	const double n = checkNumber(L, 2);

	lua_pushnumber(L, x - std::floor(x / n) * n);
	return 1;
}

static int util_smoothstep(lua_State* L) {
	const double edge0 = checkNumber(L, 1);
	const double edge1 = checkNumber(L, 2);
	const double t = std::clamp((checkNumber(L, 3) - edge0) / (edge1 - edge0), 0.0, 1.0);

	lua_pushnumber(L, t * t * (3.0 - 2.0 * t));
	return 1;
}

static int util_smootherstep(lua_State* L) {
	const double edge0 = checkNumber(L, 1);
	const double edge1 = checkNumber(L, 2);
	const double t = std::clamp((checkNumber(L, 3) - edge0) / (edge1 - edge0), 0.0, 1.0);

	lua_pushnumber(L, t * t * t * (t * (t * 6.0 - 15.0) + 10.0));
	return 1;
}

static int util_easing(lua_State* L) {
	const char* name = luaL_checkstring(L, 1);
	const double t = checkNumber(L, 2);

	for (const Easing& easing : EASINGS) {
		if (std::strcmp(easing.name, name) == 0) {
			lua_pushnumber(L, easing.function(t));
			return 1;
		}
	}

	return luaL_error(L, "unknown easing '%s'", name);
}

void wrap_sm_util(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.util' library...");

	static const luaL_Reg functions[] = {
		{"clamp", util_clamp},
		{"lerp", util_lerp},
		{"positiveModulo", util_positiveModulo},
		{"smoothstep", util_smoothstep},
		{"smootherstep", util_smootherstep},
		{"easing", util_easing},

		{nullptr, nullptr}
	};

	lua_newtable(L);
	luaL_register(L, nullptr, functions);
	lua_setfield(L, -2, "util");
}
