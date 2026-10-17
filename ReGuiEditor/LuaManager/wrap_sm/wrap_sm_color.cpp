#include "wrap_sm_color.hpp" // VCIC-Excluded

#include "../UserdataManager.hpp"

#include <rend/Common/Color.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

using rend::Color;

inline static Color& checkColor(lua_State* L, int index) {
	return UserdataManager::getInstance().check<Color>(L, index);
}

inline static float checkFloat(lua_State* L, int index) {
	return static_cast<float>(luaL_checknumber(L, index));
}

inline static int pushColor(lua_State* L, const Color& value) {
	UserdataManager::getInstance().create<Color>(L, value);
	return 1;
}

inline static int pushFloat(lua_State* L, float value) {
	lua_pushnumber(L, value);
	return 1;
}

// "RRGGBB" or "RRGGBBAA", optionally prefixed with '#'.
[[nodiscard]] inline static bool parseHex(const char* text, Color& output) {
	if (*text == '#')
		++text;

	const size_t length = std::strlen(text);
	if (length != 6 && length != 8)
		return false;

	uint32_t value = 0;
	for (size_t i = 0; i < length; ++i) {
		const char c = text[i];

		uint32_t digit;
		if (c >= '0' && c <= '9')      digit = c - '0';
		else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
		else return false;

		value = (value << 4) | digit;
	}

	if (length == 6)
		value = (value << 8) | 0xFF;

	output = { ((value >> 24) & 0xFF) / 255.0f, ((value >> 16) & 0xFF) / 255.0f, ((value >> 8) & 0xFF) / 255.0f, (value & 0xFF) / 255.0f };
	return true;
}

inline static uint8_t toByte(float channel) {
	return static_cast<uint8_t>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f));
}

// Constructors

// new(r, g, b [, a]) | new("RRGGBB[AA]") | new(0xRRGGBBAA)
static int color_new(lua_State* L) {
	const int argumentCount = lua_gettop(L);

	if (argumentCount == 1 && lua_type(L, 1) == LUA_TSTRING) {
		Color color;
		if (!parseHex(lua_tostring(L, 1), color))
			return luaL_error(L, "invalid hex color '%s'", lua_tostring(L, 1));

		return pushColor(L, color);
	}

	if (argumentCount == 1) {
		const uint32_t value = static_cast<uint32_t>(static_cast<uint64_t>(luaL_checknumber(L, 1)));

		return pushColor(L, { ((value >> 24) & 0xFF) / 255.0f, ((value >> 16) & 0xFF) / 255.0f, ((value >> 8) & 0xFF) / 255.0f, (value & 0xFF) / 255.0f });
	}

	if (argumentCount != 3 && argumentCount != 4)
		return luaL_error(L, "sm.color.new expects 1, 3 or 4 arguments");

	return pushColor(L, { checkFloat(L, 1), checkFloat(L, 2), checkFloat(L, 3), argumentCount == 4 ? checkFloat(L, 4) : 1.0f });
}

// Components

static int color_getR(lua_State* L) { return pushFloat(L, checkColor(L, 1).r); }
static int color_getG(lua_State* L) { return pushFloat(L, checkColor(L, 1).g); }
static int color_getB(lua_State* L) { return pushFloat(L, checkColor(L, 1).b); }
static int color_getA(lua_State* L) { return pushFloat(L, checkColor(L, 1).a); }

static int color_setR(lua_State* L) { checkColor(L, 1).r = checkFloat(L, 2); return 0; }
static int color_setG(lua_State* L) { checkColor(L, 1).g = checkFloat(L, 2); return 0; }
static int color_setB(lua_State* L) { checkColor(L, 1).b = checkFloat(L, 2); return 0; }
static int color_setA(lua_State* L) { checkColor(L, 1).a = checkFloat(L, 2); return 0; }

// Conversion

static int color_getHexStr(lua_State* L) {
	const Color& color = checkColor(L, 1);

	char buffer[9];
	std::snprintf(buffer, sizeof(buffer), "%02x%02x%02x%02x", toByte(color.r), toByte(color.g), toByte(color.b), toByte(color.a));

	lua_pushstring(L, buffer);
	return 1;
}

// Metamethods

static int color_add(lua_State* L) { return pushColor(L, checkColor(L, 1) + checkColor(L, 2)); }
static int color_sub(lua_State* L) { return pushColor(L, checkColor(L, 1) - checkColor(L, 2)); }

static int color_mul(lua_State* L) {
	if (lua_isnumber(L, 1))
		return pushColor(L, checkColor(L, 2) * checkFloat(L, 1));

	if (lua_isnumber(L, 2))
		return pushColor(L, checkColor(L, 1) * checkFloat(L, 2));

	return pushColor(L, checkColor(L, 1) * checkColor(L, 2));
}

static int color_div(lua_State* L) {
	if (lua_isnumber(L, 2))
		return pushColor(L, checkColor(L, 1) / checkFloat(L, 2));

	return pushColor(L, checkColor(L, 1) / checkColor(L, 2));
}

static int color_eq(lua_State* L) {
	lua_pushboolean(L, checkColor(L, 1) == checkColor(L, 2));
	return 1;
}

static int color_tostring(lua_State* L) {
	const Color& color = checkColor(L, 1);

	char buffer[128];
	std::snprintf(buffer, sizeof(buffer), "Color( %g, %g, %g, %g )", color.r, color.g, color.b, color.a);

	lua_pushstring(L, buffer);
	return 1;
}

void wrap_sm_color(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.color' library...");

	// Exposed both as sm.color.fn(c, ...) and c:fn(...).
	static const luaL_Reg functions[] = {
		{"getR", color_getR}, {"getG", color_getG}, {"getB", color_getB}, {"getA", color_getA},
		{"setR", color_setR}, {"setG", color_setG}, {"setB", color_setB}, {"setA", color_setA},
		{"getHexStr", color_getHexStr},
		{nullptr, nullptr}
	};

	static const luaL_Reg constructors[] = {
		{"new", color_new},
		{nullptr, nullptr}
	};

	UserdataDefinition* definition = UserdataManager::getInstance().createDefinition("Color", 1002);
	definition->bind<Color>();

	for (const luaL_Reg* function = functions; function->name; ++function)
		definition->addMethod(function->name, function->func);

	definition->addGetter("r", color_getR);
	definition->addGetter("g", color_getG);
	definition->addGetter("b", color_getB);
	definition->addGetter("a", color_getA);

	definition->addSetter("r", color_setR);
	definition->addSetter("g", color_setG);
	definition->addSetter("b", color_setB);
	definition->addSetter("a", color_setA);

	definition->addMetamethod("__add", color_add);
	definition->addMetamethod("__sub", color_sub);
	definition->addMetamethod("__mul", color_mul);
	definition->addMetamethod("__div", color_div);
	definition->addMetamethod("__eq", color_eq);
	definition->addMetamethod("__tostring", color_tostring);

	lua_newtable(L);
	luaL_register(L, nullptr, constructors);
	luaL_register(L, nullptr, functions);
	lua_setfield(L, -2, "color");
}
