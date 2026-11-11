#pragma once
#include "LuaClassInstance.hpp"
#include "LuaVM.hpp"

#include "UserdataManager.hpp"

#include <UTILS/Singleton.hpp>
#include <UTILS/Uuid.hpp>

#include <LuaJIT/lua.h>

#include <memory>
#include <vector>

class LuaManager : public UTILS::Singleton<LuaManager> {
	lua_State* state_ = nullptr;
	std::vector<std::unique_ptr<LuaVM>> luaVMs_;

	std::shared_ptr<UserdataManager> userdataManager_ = nullptr;

	LuaClassInstance* loader_ = nullptr;

	LuaVM* createVM(const UTILS::UUID& modLocalId);
	LuaClassInstance* loadMod(const UTILS::UUID& modLocalId);
public:
	LuaManager() = default;
	~LuaManager() { this->cleanup(); }

	bool initialize();
	void cleanup();

	void update(float deltaTime);
	void tick();

	[[nodiscard]] LuaClassInstance* getLoaderClassInstance() const { return this->loader_; }

	[[nodiscard]] LuaVM* getVM(const UTILS::UUID& modLocalId) const;
	[[nodiscard]] LuaVM* getReGuiVM() const { return this->loader_ ? &this->loader_->getVM() : nullptr; }
};
