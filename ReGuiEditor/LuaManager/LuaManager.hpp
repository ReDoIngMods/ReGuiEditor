#pragma once
#include "LuaClassInstance.hpp"
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

	std::shared_ptr<UserdataManager> userdataManager_ = nullptr;

	LuaClassInstance* loader_ = nullptr;

	LuaVM* createVM(const std::string& modLocalId);
	LuaClassInstance* loadMod(const std::string& modLocalId);
public:
	LuaManager() = default;
	~LuaManager() { this->cleanup(); }

	bool initialize();
	void cleanup();

	void update(float deltaTime);
	void tick();

	[[nodiscard]] LuaClassInstance* getLoaderClassInstance() const { return this->loader_; }

	[[nodiscard]] LuaVM* getVM(const std::string& modLocalId) const;
	[[nodiscard]] LuaVM* getReGuiVM() const { return this->loader_ ? &this->loader_->getVM() : nullptr; }
};
