#include "LuaManager.hpp"

#include "LuaClassInstance.hpp"
#include "LuaUtil.hpp"
#include "LuaVM.hpp"

#include "UserdataManager.hpp"

#include "wrap_global.hpp"
#include "wrap_sm/wrap_sm.hpp"

#include <Filesystem/DirectoryManager.hpp>
#include <Filesystem/FileReader.hpp>

#include <MyGuiRenderer/JsonGuiInstance.hpp>

#include <JSON/JSON.hpp>
#include <JSON/JSONSerializable.hpp>

#include <UTILS/Uuid.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>
#include <LuaJIT/lualib.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

static const UTILS::UUID REGUI_MOD_LOCALID = "0e0779e3-a4ae-46c7-89db-1596a935180d";

constexpr const char* ENV_SCRIPT_PATH = "$RGE_DATA/Scripts/EnvVars.lua";
constexpr const char* MOD_CONFIG_PATH = "$CONTENT_DATA/reguieditor.config.json";

static int luaPanic(lua_State* L) {
	const char* message = lua_tostring(L, 1);
	CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "PANIC: %s", message ? message : "unknown error");

	LuaUtils::dumpStack(L);
	return 0;
}

static bool defineEnvironment(lua_State* L) {
	auto content = Filesystem::readFileTextUTF8(ENV_SCRIPT_PATH);
	if (!content)
		return false;

	if (luaL_loadbufferx(L, content->data(), content->size(), ENV_SCRIPT_PATH, "t") != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
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
		CONSOLE_ERROR("Failed to create Lua state.");
		return false;
	}

	lua_atpanic(L, luaPanic);

	struct LuaLib {
		const char* displayName;
		const char* name;
		lua_CFunction openFunc;
	};

	constexpr LuaLib luaLibs[] = {
		{"base",    "",              luaopen_base   },
		{"package", LUA_LOADLIBNAME, luaopen_package},
		{"table",   LUA_TABLIBNAME,  luaopen_table  },
		{"io",      LUA_IOLIBNAME,   luaopen_io     },
		{"os",      LUA_OSLIBNAME,   luaopen_os     },
		{"string",  LUA_STRLIBNAME,  luaopen_string },
		{"math",    LUA_MATHLIBNAME, luaopen_math   },
		{"debug",   LUA_DBLIBNAME,   luaopen_debug  },
		{"bit",     LUA_BITLIBNAME,  luaopen_bit    },
		{"jit",     LUA_JITLIBNAME,  luaopen_jit    }, // KEEP! Otherwise we will not have JIT compilation!
		{"ffi",     LUA_FFILIBNAME,  luaopen_ffi    },
	};

	for (const auto& lib : luaLibs) {
		lua_pushcfunction(L, lib.openFunc);
		lua_pushstring(L, lib.name);
		lua_call(L, 1, 0);

		CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Loaded Lua library: %s", lib.displayName);
	}

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

	LuaClassInstance* loader = this->loadMod(REGUI_MOD_LOCALID);
	if (!loader || loader->getType() != LuaClassType::Loader) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "sm.regui must provide a Loader class.");

		lua_close(L);
		return false;
	}

	this->loader_ = loader;

	return true;
}

LuaVM* LuaManager::createVM(const UTILS::UUID& modLocalId) {
	auto iterator = std::find_if(this->luaVMs_.begin(), this->luaVMs_.end(), [&modLocalId](const std::unique_ptr<LuaVM>& vm) {
		return vm->getModLocalId() == modLocalId;
	});

	if (iterator != this->luaVMs_.end()) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "Lua VM for mod %s already exists. Skipping creation.", modLocalId.toAStr().c_str());
		return nullptr;
	}

	auto luaVM = std::make_unique<LuaVM>(this->state_, modLocalId);
	if (!luaVM->initialize()) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Failed to initialize Lua VM for mod %s.", modLocalId.toAStr().c_str());
		return nullptr;
	}

	this->luaVMs_.push_back(std::move(luaVM));
	return this->luaVMs_.back().get();
}

void LuaManager::cleanup() {
	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Cleaning up LuaManager...");

	this->loader_ = nullptr;
	this->luaVMs_.clear();

	if (this->state_) {
		lua_close(this->state_);
		this->state_ = nullptr;
	}
}

void LuaManager::update(float deltaTime) {
	JsonGuiInstance::dispatchPendingEvents();

	for (const auto& luaVM : this->luaVMs_)
		luaVM->update(deltaTime);

	if (!this->loader_ || !this->loader_->exists())
		return;

	lua_pushnumber(this->state_, deltaTime);
	(void)this->loader_->callMethod("update", 1, 0);
}

void LuaManager::tick() {
	if (!this->loader_ || !this->loader_->exists())
		return;

	(void)this->loader_->callMethod("tick", 0, 0);
}

LuaVM* LuaManager::getVM(const UTILS::UUID& modLocalId) const {
	auto iterator = std::find_if(this->luaVMs_.begin(), this->luaVMs_.end(), [&modLocalId](const std::unique_ptr<LuaVM>& vm) {
		return vm->getModLocalId() == modLocalId;
	});

	return (iterator != this->luaVMs_.end()) ? iterator->get() : nullptr;
}

struct ModConfigScript : public JSONSerializable {
	std::string filename;
	std::string classname;

	JSONValue serialize() const override {
		JSONObject object;
		object["filename"] = this->filename;
		object["classname"] = this->classname;
		
		return std::move(object);
	}

	bool deserialize(const JSONValue& value) override {
		if (!this->getValue(value, "filename", this->filename)) return false;
		if (!this->getValue(value, "classname", this->classname)) return false;
		return true;
	}
};

struct ModConfig : public JSONSerializable {
	constexpr static int VERSION = 1;

	LuaClassType type = LuaClassType::Addon;
	ModConfigScript script;

	JSONValue serialize() const override {
		JSONObject object;
		object["version"] = VERSION;
		object["type"] = (this->type == LuaClassType::Loader) ? "Loader" : "Addon";
		object["script"] = this->script.serialize();

		return std::move(object);
	}

	bool deserialize(const JSONValue& value) override {
		if (!value.isObject())
			return false;

		if (!this->verifyValue(value, "version", VERSION)) return false;
		
		std::string typeStr;
		if (!this->getValue(value, "type", typeStr))
			return false;

		if (typeStr == "Loader")
			this->type = LuaClassType::Loader;
		else if (typeStr == "Addon")
			this->type = LuaClassType::Addon;
		else
			return false;

		if (!this->getValue(value, "script", this->script))
			return false;

		return true;
	}
};

LuaClassInstance* LuaManager::loadMod(const UTILS::UUID& modLocalId) {
	std::filesystem::path configPath = MOD_CONFIG_PATH;
	DirectoryManager::resolveContentDataStatic(configPath, modLocalId);

	auto configJson = Filesystem::readFileJson(configPath);
	if (!configJson) {
		//CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Failed to read config for mod %s.", modLocalId.c_str());
		CONSOLE_ERROR("Failed to read config for mod %s.", modLocalId.toAStr().c_str());
		return nullptr;
	}

	ModConfig config;
	if (!config.deserialize(configJson.value())) {
		CONSOLE_ERROR("Invalid config for mod %s.", modLocalId.toAStr().c_str());
		return nullptr;
	}

	if (config.type == LuaClassType::Loader && modLocalId != REGUI_MOD_LOCALID) {
		CONSOLE_ERROR("Mod %s cannot use the Loader type, it is reserved for sm.regui.", modLocalId.toAStr().c_str());
		return nullptr;
	}

	LuaVM* vm = this->createVM(modLocalId);
	if (!vm)
		return nullptr;

	LuaClassInstance* classInstance = vm->loadClass(config.type, config.script.filename, config.script.classname);
	if (!classInstance) {
		CONSOLE_ERROR("Failed to load class %s for mod %s.", config.script.classname.c_str(), modLocalId.toAStr().c_str());

		this->luaVMs_.pop_back();
		return nullptr;
	}

	return classInstance;
}