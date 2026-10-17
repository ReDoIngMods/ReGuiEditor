#pragma once
#include "LuaVM.hpp"
#include "UserdataManager.hpp"

#include <UTILS/Singleton.hpp>

#include <LuaJIT/lua.h>

#include <memory>
#include <string>
#include <vector>

class LuaManager : public UTILS::Singleton<LuaManager> {
	lua_State* state_ = nullptr;
	std::vector<std::unique_ptr<LuaVM>> luaVMs_;

	UserdataManager* userdataManager_ = nullptr;

	LuaVM* reGuiVM_ = nullptr;

	LuaVM* createVM(const std::string& modLocalId, const std::string& entryScript);
public:
	LuaManager() = default;
	~LuaManager() { this->cleanup(); }

	LuaManager(const LuaManager&) = delete;
	LuaManager& operator=(const LuaManager&) = delete;
	LuaManager(LuaManager&&) = delete;
	LuaManager& operator=(LuaManager&&) = delete;

	bool initialize();
	void cleanup();

	void update(float deltaTime);
	void tick();

	[[nodiscard]] LuaVM* getVM(const std::string& modLocalId) const;
	[[nodiscard]] LuaVM* getReGuiVM() const { return this->reGuiVM_; }
};
