#pragma once
#include <UTILS/Singleton.hpp>

#include <LuaJIT/lua.h>

#include <string>

class LuaManager : public UTILS::Singleton<LuaManager> {
	lua_State* luaState_ = nullptr;
public:
	LuaManager() = default;
	~LuaManager() { this->cleanup(); }

	LuaManager(const LuaManager&) = delete;
	LuaManager& operator=(const LuaManager&) = delete;
	LuaManager(LuaManager&&) = delete;
	LuaManager& operator=(LuaManager&&) = delete;

	bool initialize();
	void cleanup();

	lua_State* getLuaState() const { return this->luaState_; }

	bool checkStringArgument(lua_State* L, int index, std::string& outString);
};