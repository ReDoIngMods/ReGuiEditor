#include "LuaManager.hpp"

#include <FilesystemManager/FilesystemManager.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>
#include <LuaJIT/lualib.h>
#include <string>

static int print(lua_State* L) {
	if (int argumentCount = lua_gettop(L); argumentCount == 1) {
		luaL_error(L, "Expected 1 argument, got %d.", argumentCount);
		return 0;
	}

	std::string output;

	LuaManager& luaManager = LuaManager::getInstance();
	luaManager.checkStringArgument(L, 1, output);

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "%s", output.c_str());

	return 0;
}

bool LuaManager::initialize() {
	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Initializing LuaManager...");

	this->luaState_ = luaL_newstate();
	if (!this->luaState_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create Lua state.");
		return false;
	}

	luaL_openlibs(this->luaState_);
	
	lua_pushcfunction(this->luaState_, print);
	lua_setglobal(this->luaState_, "print");

	lua_newtable(this->luaState_);
	lua_setglobal(this->luaState_, "sm");

	lua_newtable(this->luaState_);
	lua_setglobal(this->luaState_, "reguieditor");
	
	return true;
}

void LuaManager::cleanup() {
	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Cleaning up LuaManager...");

	if (this->luaState_) {
		lua_close(this->luaState_);
		this->luaState_ = nullptr;
	}
}

bool LuaManager::checkStringArgument(lua_State* L, int index, std::string& outString) {
	if (!lua_isstring(L, index)) {
		luaL_error(L, "Expected a string argument at index %d.", index);
		return false;
	}

	size_t length;
	const char* str = lua_tolstring(L, index, &length);
	outString.assign(str, length);

	return true;
}