#include "LuaManager.hpp"

#include <FilesystemManager/FilesystemManager.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>
#include <LuaJIT/lualib.h>
#include <string>

// TODO:
//		- Implement hot-reloading of Lua scripts.
//		- Implement a way to register libraries and functions to Lua in a nice way.
//		- Deshittify this class.
//		- Implement proper file management so we get nice error messages and call stack dumps
//		- Comminucate with the sm.regui lua mod.

static int print(lua_State* L) {
	if (int argumentCount = lua_gettop(L); argumentCount != 1) {
		luaL_error(L, "Expected 1 argument, got %d.", argumentCount);
		return 0;
	}

	std::string output;

	LuaManager& luaManager = LuaManager::getInstance();
	luaManager.checkStringArgument(L, 1, output);

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "%s", output.c_str());

	return 0;
}

static int dofile(lua_State* L) {
	if (int argumentCount = lua_gettop(L); argumentCount != 1) {
		luaL_error(L, "Expected 1 argument, got %d.", argumentCount);
		return 0;
	}

	std::string filePath;

	LuaManager& luaManager = LuaManager::getInstance();
	luaManager.checkStringArgument(L, 1, filePath);

	if (!FilesystemManager::getInstance().fileExists(filePath)) {
		luaL_error(L, "File does not exist: %s", filePath.c_str());
		return 0;
	}

	if (luaL_dofile(L, filePath.c_str()) != LUA_OK) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "ERROR: %s %s", filePath.c_str(), lua_tostring(L, -1));
		
		luaManager.dumpStack(L);
		return 0;
	}
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

	lua_pushcfunction(this->luaState_, dofile);
	lua_setglobal(this->luaState_, "dofile");

	lua_newtable(this->luaState_);
	lua_setglobal(this->luaState_, "sm");

	lua_newtable(this->luaState_);
	lua_setglobal(this->luaState_, "reguieditor");
	
	if (luaL_dostring(this->luaState_, "error('Error test')") != LUA_OK) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "ERROR: %s %s", "<Custom>", lua_tostring(this->luaState_, -1));
		this->dumpStack(this->luaState_);
		this->dumpCallStack(this->luaState_);
	}

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

void LuaManager::dumpStack(lua_State* L) {
	int top = lua_gettop(L);
	CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "--- LUA STACK DUMP ---");

	for (int index = 1; index <= top; ++index) {
		if (lua_isstring(L, index)) {
			size_t length;
			const char* str = lua_tolstring(L, index, &length);
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %.*s", index, static_cast<int>(length), str);
		} else {
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %s", index, luaL_typename(L, index));
		}
	}
}

void LuaManager::dumpCallStack(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "--- LUA CALL STACK ---");

	lua_Debug ar;
	int level = 0;

	while (lua_getstack(L, level, &ar)) {
		if (lua_getinfo(L, "nSl", &ar)) {
			const char* name = ar.name ? ar.name : "?";
			const char* source = ar.source ? ar.source : "?";

			CONSOLE_LOG(
				LEVEL_LUA,
				COLOR_YELLOW,
				"[%d] %s() - %s:%d",
				level,
				name,
				source,
				ar.currentline
			);
		}

		++level;
	}
}