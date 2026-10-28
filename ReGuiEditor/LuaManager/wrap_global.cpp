#include "wrap_global.hpp" // VCIC-Excluded

#include "LuaUtil.hpp"
#include "LuaVM.hpp"

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <string>
#include <utility>

static int global_dofile(lua_State* L) {
	LuaVM* luaVM = LuaUtils::getLuaVM(L);
	if (!luaVM)
		return luaL_error(L, "Failed to get LuaVM instance");

	std::string path = luaL_checkstring(L, 1);
	if (path.empty())
		return luaL_error(L, "dofile expects a non-empty path");

	lua_settop(L, 1);

	if (path.front() != '$') {
		std::string callerPath;
		if (!LuaUtils::getCallerPath(L, callerPath))
			return luaL_error(L, "cannot resolve relative path '%s' outside of a script file", path.c_str());

		std::string resolvedPath;
		if (!LuaUtils::joinRelativePath(callerPath, path, resolvedPath))
			return luaL_error(L, "cannot resolve relative path '%s' from '%s'", path.c_str(), callerPath.c_str());

		path = std::move(resolvedPath);
	}

	if (luaVM->loadFile(path) != LUA_OK)
		return lua_error(L);

	lua_call(L, 0, LUA_MULTRET);
	return lua_gettop(L) - 1;
}

static int global_enumType(lua_State* L) {
	if (lua_gettop(L) > 1)
		return luaL_error(L, "enumType expects at most one argument");

	const int type = lua_type(L, 1);
	if (type == LUA_TUSERDATA && luaL_getmetafield(L, 1, "__typeid")) {
		if (lua_type(L, -1) == LUA_TNUMBER)
			return 1;

		lua_pop(L, 1);
	}

	lua_pushinteger(L, type);
	return 1;
}

static int global_print(lua_State* L) {
	std::string message;
	if (const char* error = LuaUtils::argsToString(L, message))
		return luaL_error(L, "%s", error);

	CONSOLE_LOG(LEVEL_LUA, COLOR_GREEN, "%s", message.c_str());
	return 0;
}

static int global_type(lua_State* L) {
	if (lua_gettop(L) != 1)
		return luaL_error(L, "type expects exactly one argument");

	const int type = lua_type(L, 1);
	if (type == LUA_TUSERDATA && luaL_getmetafield(L, 1, "__name")) {
		if (lua_type(L, -1) == LUA_TSTRING)
			return 1;

		lua_pop(L, 1);
	}

	lua_pushstring(L, lua_typename(L, type));
	return 1;
}

void wrap_global(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'global' library...");

	static const luaL_Reg functions[] = {
		{"dofile", global_dofile},
		{"enumType", global_enumType},
		{"print", global_print},
		{"type", global_type},

		{nullptr, nullptr}
	};

	lua_pushvalue(L, LUA_GLOBALSINDEX);
	luaL_register(L, nullptr, functions);
	lua_pop(L, 1);
}
