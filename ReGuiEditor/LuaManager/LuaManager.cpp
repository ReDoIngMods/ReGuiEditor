#include "LuaManager.hpp"
#include "LuaVM.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#define REGUI_MOD_LOCALID "0e0779e3-a4ae-46c7-89db-1596a935180d"

bool LuaManager::initialize() {
	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Initializing LuaManager...");

	if (this->getVM(REGUI_MOD_LOCALID))
		return true;

	return this->createVM(REGUI_MOD_LOCALID, "$CONTENT_" REGUI_MOD_LOCALID "/Scripts/Application/Loader.lua");
}

bool LuaManager::createVM(const std::string& modLocalId, const std::string& entryScript) {
	auto iterator = std::find_if(this->vms_.begin(), this->vms_.end(), [&modLocalId](const std::unique_ptr<LuaVM>& vm) {
		return vm->getModLocalId() == modLocalId;
	});

	if (iterator != this->vms_.end()) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "Lua VM for mod %s already exists. Skipping creation.", modLocalId.c_str());
		return true;
	}

	auto vm = std::make_unique<LuaVM>(modLocalId);
	if (!vm->initialize(entryScript)) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Failed to initialize Lua VM for mod %s.", modLocalId.c_str());
		return false;
	}

	this->vms_.push_back(std::move(vm));
	return true;
}

void LuaManager::cleanup() {
	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Cleaning up LuaManager...");

	this->vms_.clear();
}

void LuaManager::update(float deltaTime) {
	for (const auto& vm : this->vms_)
		vm->update(deltaTime);
}

LuaVM* LuaManager::getVM(const std::string& modLocalId) const {
	auto iterator = std::find_if(this->vms_.begin(), this->vms_.end(), [&modLocalId](const std::unique_ptr<LuaVM>& vm) {
		return vm->getModLocalId() == modLocalId;
	});

	return (iterator != this->vms_.end()) ? iterator->get() : nullptr;
}
