#include "LuaManager.hpp"
#include "LuaVM.hpp"

#include "UserdataManager.hpp"
#include "wrap_global.hpp"
#include "wrap_sm/wrap_sm.hpp"

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>
#include <LuaJIT/lualib.h>

#include <FilesystemManager/FilesystemManager.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

constexpr const char* REGUI_MOD_LOCALID = "0e0779e3-a4ae-46c7-89db-1596a935180d";
constexpr const char* ENV_SCRIPT_PATH = "$RGE_DATA/Scripts/EnvVars.lua";

static int luaPanic(lua_State* L) {
	const char* message = lua_tostring(L, 1);
	CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "PANIC: %s", message ? message : "unknown error");

	LuaVM::dumpStack(L);
	return 0;
}

static bool defineEnvironment(lua_State* L) {
	FilesystemManager& filesystemManager = FilesystemManager::getInstance();

	std::string path = ENV_SCRIPT_PATH;
	std::vector<char> content;
	if (!filesystemManager.readFile(path, content)) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Failed to read environment script %s.", ENV_SCRIPT_PATH);
		return false;
	}

	if (luaL_loadbuffer(L, content.data(), content.size(), ENV_SCRIPT_PATH) != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
		const char* message = lua_tostring(L, -1);
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "ERROR: %s", message ? message : "unknown error");

		lua_pop(L, 1);
		return false;
	}

	lua_getglobal(L, "unsafe_env");
	const bool defined = lua_istable(L, -1);
	lua_pop(L, 1);

	if (!defined)
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "%s did not define the 'unsafe_env' table.", ENV_SCRIPT_PATH);

	return defined;
}

bool LuaManager::initialize() {
	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Initializing LuaManager...");

	lua_State* L = luaL_newstate();
	if (!L) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create Lua state.");
		return false;
	}

	lua_atpanic(L, luaPanic);
	luaL_openlibs(L);

	this->userdataManager_ = UserdataManager::createInstance();

	wrap_global(L);
	wrap_sm(L);

	lua_newtable(L);

	lua_pushstring(L, "0.1.0");
	lua_setfield(L, -2, "version");

	lua_setglobal(L, "reguieditor");

	this->userdataManager_->registerDefinitions(L);

	if (!defineEnvironment(L)) {
		lua_close(L);
		return false;
	}

	this->state_ = L;

	return this->createVM(REGUI_MOD_LOCALID, "$CONTENT_DATA/Scripts/Application/Loader.lua");
}

bool LuaManager::createVM(const std::string& modLocalId, const std::string& entryScript) {
	auto iterator = std::find_if(this->luaVMs_.begin(), this->luaVMs_.end(), [&modLocalId](const std::unique_ptr<LuaVM>& vm) {
		return vm->getModLocalId() == modLocalId;
	});

	if (iterator != this->luaVMs_.end()) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "Lua VM for mod %s already exists. Skipping creation.", modLocalId.c_str());
		return true;
	}

	auto luaVM = std::make_unique<LuaVM>(this->state_, modLocalId);
	if (!luaVM->initialize(entryScript)) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Failed to initialize Lua VM for mod %s.", modLocalId.c_str());
		return false;
	}

	this->luaVMs_.push_back(std::move(luaVM));
	return true;
}

void LuaManager::cleanup() {
	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Cleaning up LuaManager...");

	this->luaVMs_.clear();

	if (this->state_) {
		lua_close(this->state_);
		this->state_ = nullptr;
	}

	if (this->userdataManager_) {
		this->userdataManager_->destroyInstance();
		this->userdataManager_ = nullptr;
	}
}

void LuaManager::update(float deltaTime) {
	for (const auto& luaVM : this->luaVMs_)
		luaVM->update(deltaTime);
}

LuaVM* LuaManager::getVM(const std::string& modLocalId) const {
	auto iterator = std::find_if(this->luaVMs_.begin(), this->luaVMs_.end(), [&modLocalId](const std::unique_ptr<LuaVM>& vm) {
		return vm->getModLocalId() == modLocalId;
	});

	return (iterator != this->luaVMs_.end()) ? iterator->get() : nullptr;
}
