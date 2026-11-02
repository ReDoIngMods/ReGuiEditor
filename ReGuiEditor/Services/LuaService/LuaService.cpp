#include "LuaService.hpp"

#include "LuaModManifest.hpp"
#include "LuaResult.hpp"
#include "LuaState.hpp"
#include "LuaVM.hpp"

#include <Filesystem/FilesystemManager.hpp>

#include <Modules/UniqueIdentifiers/Uuid.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

static constexpr const char* ENV_SCRIPT_PATH = "$RGE_DATA/Scripts/EnvVars.lua";

bool ReGuiEditor::LuaService::defineEnvironment() {
	std::vector<char> content;
	if (!FilesystemManager::getInstance().readFile(ENV_SCRIPT_PATH, content)) {
		CONSOLE_ERROR("Failed to read environment script %s.", ENV_SCRIPT_PATH);
		return false;
	}

	lua_State* L = this->luaState_->getState();
	if (luaL_loadbufferx(L, content.data(), content.size(), ENV_SCRIPT_PATH, "t") != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
		const char* message = lua_tostring(L, -1);
		CONSOLE_ERROR("ERROR: %s", message ? message : "unknown error");
	
		lua_pop(L, 1);
		return false;
	}

	lua_getglobal(L, "unsafe_env");
	const bool defined = lua_istable(L, -1);
	if (!defined)
		CONSOLE_ERROR("%s did not define the 'unsafe_env' table.", ENV_SCRIPT_PATH);

	lua_pop(L, 1);
	return defined;
}

bool ReGuiEditor::LuaService::initialize() {
	this->luaState_ = std::make_unique<LuaState>();
	if (!this->luaState_->initialize()) {
		CONSOLE_ERROR("Failed to initialize Lua state.");
		return false;
	}

	if (!this->defineEnvironment())
		return false;

	return true;
}

void ReGuiEditor::LuaService::cleanup() {
	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Cleaning up LuaService...");

	this->virtualMachines_.clear();
	this->luaState_.reset();
}

void ReGuiEditor::LuaService::update(double deltaTime) {
	for (const auto& virtualMachine : this->virtualMachines_)
		virtualMachine->update(static_cast<float>(deltaTime));
}

void ReGuiEditor::LuaService::tick() {
	for (const auto& virtualMachine : this->virtualMachines_)
		virtualMachine->tick();
}

ReGuiEditor::LuaVM* ReGuiEditor::LuaService::createVirtualMachine(const ReGuiEditor::Uuid& modLocalId) {
	const std::string localId = modLocalId.toString();
	if (this->getVirtualMachine(modLocalId)) {
		CONSOLE_WARNING("Lua VM for mod %s already exists. Skipping creation.", localId.c_str());
		return nullptr;
	}

	if (!ReGuiEditor::LuaModManifest::exists(modLocalId)) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Mod %s has no %s, it does not support ReGuiEditor.", localId.c_str(), ReGuiEditor::LuaModManifest::FILE_PATH);
		return nullptr;
	}

	ReGuiEditor::LuaModManifest manifest;
	std::string error;
	if (!LuaModManifest::load(modLocalId, manifest, error)) {
		CONSOLE_ERROR("Invalid %s in mod %s: %s.", LuaModManifest::FILE_PATH, localId.c_str(), error.c_str());
		return nullptr;
	}

	auto virtualMachine = std::make_unique<LuaVM>(*this->luaState_, modLocalId);
	if (!virtualMachine->initialize()) {
		CONSOLE_ERROR("Failed to initialize Lua VM for mod %s.", localId.c_str());
		return nullptr;
	}

	ReGuiEditor::LuaResult result = virtualMachine->doFile(manifest.scriptFilename);
	if (!result) {
		result.report();
		return nullptr;
	}

	result = virtualMachine->createInstance(manifest.className);
	if (!result) {
		result.report();
		return nullptr;
	}

	this->virtualMachines_.push_back(std::move(virtualMachine));
	return this->virtualMachines_.back().get();
}

ReGuiEditor::LuaVM* ReGuiEditor::LuaService::getVirtualMachine(const ReGuiEditor::Uuid& modLocalId) const {
	const auto iterator = std::find_if(this->virtualMachines_.begin(), this->virtualMachines_.end(), [&modLocalId](const std::unique_ptr<ReGuiEditor::LuaVM>& virtualMachine) {
		return virtualMachine->getModLocalId() == modLocalId;
	});

	if (iterator == this->virtualMachines_.end())
		return nullptr;

	return iterator->get();
}
