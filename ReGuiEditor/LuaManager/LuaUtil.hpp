#pragma once
#include <LuaJIT/lua.h>

#include <string>

namespace LuaUtils {
	// Joins every argument on the stack into one space-separated string, honouring
	// string-valued and function-valued __tostring.
	// 
	// Returns nullptr on success, otherwise an error message for luaL_error.
	[[nodiscard]] const char* argsToString(lua_State* L, std::string& output);
}
