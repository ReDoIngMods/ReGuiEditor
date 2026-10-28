#pragma once
#include <LuaJIT/lua.h>

#include <string>

// These are utility functions that are used across the LuaVM & wrappers. Its a lot neater like this... i think i don't know
// i need to figure out how to be better at structuring code.

class LuaVM;

namespace LuaUtils {
	[[nodiscard]] const char* argsToString(lua_State* L, std::string& output);
	[[nodiscard]] bool joinRelativePath(const std::string& baseFile, const std::string& relativePath, std::string& output);
	[[nodiscard]] bool getCallerPath(lua_State* L, std::string& output);
	[[nodiscard]] bool resolveContentDataPath(lua_State* L, std::string& path);

	[[nodiscard]] LuaVM* getLuaVM(lua_State* L);
	void setLuaVM(lua_State* L, LuaVM* vm);

	void dumpStack(lua_State* L);
}