#include "LuaUtil.hpp" // VCIC-Excluded

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <string>

inline static const char* pushAsString(lua_State* L, int index, size_t& length) {
	if (lua_isuserdata(L, index) && luaL_getmetafield(L, index, "__tostring")) {
		if (lua_type(L, -1) == LUA_TSTRING)
			return lua_tolstring(L, -1, &length);

		lua_pop(L, 1);
	}

	lua_getglobal(L, "tostring");
	lua_pushvalue(L, index);
	lua_call(L, 1, 1);

	return lua_tolstring(L, -1, &length);
}

const char* LuaUtils::argsToString(lua_State* L, std::string& output) {
	output.clear();

	for (int index = 1, top = lua_gettop(L); index <= top; ++index) {
		if (index > 1)
			output += ' ';

		size_t length = 0;
		const char* str = pushAsString(L, index, length);
		if (!str) {
			lua_pop(L, 1);
			return "'tostring' must return a string";
		}

		output.append(str, length);
		lua_pop(L, 1);
	}

	return nullptr;
}
