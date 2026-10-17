#pragma once
#include <LuaJIT/lua.h>

#include <string>

namespace LuaUtils {
	// Joins every argument on the stack into one space-separated string, honouring
	// string-valued and function-valued __tostring.
	// 
	// Returns nullptr on success, otherwise an error message for luaL_error.
	[[nodiscard]] const char* argsToString(lua_State* L, std::string& output);

	// Resolves `relativePath` against the directory of `baseFile` ("$KEY/dir/file.lua"),
	// collapsing "." and ".." segments. Fails when ".." would climb out of the "$KEY" root.
	[[nodiscard]] bool joinRelativePath(const std::string& baseFile, const std::string& relativePath, std::string& output);
}
