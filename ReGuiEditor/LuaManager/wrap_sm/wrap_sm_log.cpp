#include "wrap_sm_log.hpp" // VCIC-Excluded

#include "../LuaUtil.hpp"

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <string>

static int sm_log_info(lua_State* L) {
	std::string message;
	if (const char* error = LuaUtils::argsToString(L, message))
		return luaL_error(L, "%s", error);

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "%s", message.c_str());
	return 0;
}

static int sm_log_warning(lua_State* L) {
	std::string message;
	if (const char* error = LuaUtils::argsToString(L, message))
		return luaL_error(L, "%s", error);

	CONSOLE_WARNING("%s", message.c_str());
	return 0;
}

static int sm_log_error(lua_State* L) {
	std::string message;
	if (const char* error = LuaUtils::argsToString(L, message))
		return luaL_error(L, "%s", error);

	CONSOLE_ERROR("%s", message.c_str());
	return 0;
}

void wrap_sm_log(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.log' library...");

	static const luaL_Reg functions[] = {
		{"info", sm_log_info},
		{"warning", sm_log_warning},
		{"error", sm_log_error},

		{nullptr, nullptr}
	};

	lua_newtable(L);
	luaL_register(L, nullptr, functions);
	lua_setfield(L, -2, "log");
}
