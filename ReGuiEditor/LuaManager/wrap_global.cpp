#include "wrap_global.hpp" // VCIC-Excluded

#include "LuaUtil.hpp"
#include "LuaVM.hpp"

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <string>

static int global_class_metatable__call(lua_State* L) {
	lua_newtable(L);
	lua_pushvalue(L, 1);
	lua_setmetatable(L, -2);

	return 1;
}

static int global_class(lua_State* L) {
	const int argCount = lua_gettop(L);
	if (argCount > 1)
		return luaL_error(L, "class expects at most one argument");

	if (argCount == 1)
		luaL_checktype(L, 1, LUA_TTABLE);

	lua_newtable(L);

	if (argCount == 1) {
		const int classIndex = lua_gettop(L);

		lua_pushnil(L);
		while (lua_next(L, 1) != 0) {
			lua_pushvalue(L, -2);
			lua_insert(L, -2);
			lua_settable(L, classIndex);
		}
	}

	lua_newtable(L);

	lua_pushcfunction(L, global_class_metatable__call);
	lua_setfield(L, -2, "__call");

	lua_setmetatable(L, -2);

	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	return 1;
}

static int global_dofile(lua_State* L) {
	const char* path = luaL_checkstring(L, 1);
	lua_settop(L, 1);

	if (LuaVM::fromState(L).loadFile(path) != LUA_OK)
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
		{"class", global_class},
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
