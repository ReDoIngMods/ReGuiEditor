#include "LuaVM.hpp"

#include "LuaClassInstance.hpp"
#include "LuaError.hpp"
#include "LuaUtil.hpp"

#include <Filesystem/DirectoryManager.hpp>
#include <Filesystem/FileReader.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

constexpr const char* VM_REGISTRY_KEY = "VMPtr";
constexpr float SCRIPT_POLL_INTERVAL = 0.5f;

// Keep the VM alive after a failed entry script so the watcher can hot-reload a fixed one.
#if defined(_DEBUG) || defined(DEBUG)
	constexpr bool KEEP_ALIVE_ON_ENTRY_FAILURE = true;
#else
	constexpr bool KEEP_ALIVE_ON_ENTRY_FAILURE = false;
#endif

inline static std::string popString(lua_State* L, const char* fallback = "") {
	if (lua_isstring(L, -1)) {
		const std::string result = lua_tostring(L, -1);
		lua_pop(L, 1);

		return result;
	}

	lua_pop(L, 1);
	return fallback ? fallback : "";
}

void LuaVM::reportLoadError() {
	LuaError error;
	error.message = popString(this->state_, LuaError::kUnknownMessage);

	this->reportError(error);
}

void LuaVM::reportError(const LuaError& error) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "ERROR: %s", error.message.c_str());

	if (!error.frames.empty())
		CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "%s", error.formatTraceback().c_str());
}

int LuaVM::errorHandler(lua_State* L) {
	LuaVM* self = LuaUtils::getLuaVM(L);
	if (!self)
		return luaL_error(L, "LuaVM::errorHandler called without a valid LuaVM instance");

	self->pendingError_.reset();
	self->pendingError_.setMessage(lua_tostring(L, 1), luaL_typename(L, 1));

	lua_Debug debug;
	for (int level = 1; lua_getstack(L, level, &debug); ++level) {
		lua_getinfo(L, "nSl", &debug);

		LuaStackFrame frame;
		frame.source = debug.short_src;
		frame.function = debug.name ? debug.name : "";
		frame.what = debug.what;
		frame.line = debug.currentline;

		self->pendingError_.frames.push_back(std::move(frame));
	}

	return 1;
}


void LuaVM::createEnvironment() {
	lua_State* L = this->state_;

	lua_newtable(L);

	lua_newtable(L);
	lua_getglobal(L, "unsafe_env");
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);

	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "_G");

	lua_setfield(L, LUA_REGISTRYINDEX, this->envKey_.c_str());

	lua_pushcfunction(L, &LuaVM::errorHandler);
	this->errorHandlerRef_ = luaL_ref(L, LUA_REGISTRYINDEX);

	this->initialized_ = true;
}

void LuaVM::destroyEnvironment() {
	lua_State* L = this->state_;

	for (const auto& classInstance : this->classInstances_)
		classInstance->destroy();

	this->classInstances_.clear();

	luaL_unref(L, LUA_REGISTRYINDEX, this->errorHandlerRef_);
	this->errorHandlerRef_ = LUA_NOREF;

	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, this->envKey_.c_str());
	lua_gc(L, LUA_GCCOLLECT, 0);

	this->initialized_ = false;
	this->scriptWatcher_.clearFiles();
	this->pollTimer_ = 0.0f;
}

bool LuaVM::initialize() {
	if (this->isInitialized())
		return true;

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Initializing Lua VM for mod %s...", this->modLocalId_.toAStr().c_str());

	this->createEnvironment();
	
	return true;
}

LuaClassInstance* LuaVM::loadClass(LuaClassType type, const std::string& scriptPath, const std::string& className) {
	LuaClassInstance& classInstance = *this->classInstances_.emplace_back(std::make_unique<LuaClassInstance>(this, type, className));
	if ((this->doFile(scriptPath) && classInstance.create()) || KEEP_ALIVE_ON_ENTRY_FAILURE)
		return &classInstance;

	this->classInstances_.pop_back();
	return nullptr;
}

void LuaVM::cleanup() {
	if (!this->isInitialized())
		return;

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Cleaning up Lua VM for mod %s...", this->modLocalId_.toAStr().c_str());

	this->destroyEnvironment();
}

void LuaVM::update(float deltaTime) {
	if (!this->isInitialized())
		return;

	this->updateScriptWatcher(deltaTime);
}

void LuaVM::updateScriptWatcher(float deltaTime) {
	this->pollTimer_ += deltaTime;
	if (this->pollTimer_ < SCRIPT_POLL_INTERVAL)
		return;

	this->pollTimer_ = 0.0f;

	bool reloaded = false;
	for (const std::string& path : this->scriptWatcher_.collectChangedFiles()) {
		if (this->doFile(path))
			reloaded = true;
		else
			CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Failed to reload changed script '%s'.", path.c_str());
	}

	if (!reloaded)
		return;

	for (const auto& classInstance : this->classInstances_)
		(void)classInstance->refresh();
}

int LuaVM::loadFile(const std::string& path) {
	std::filesystem::path resolvedPath = path;
	DirectoryManager::resolveContentDataStatic(resolvedPath, this->modLocalId_);

	this->scriptWatcher_.watchFile(resolvedPath.string());

	auto fileContentOpt = Filesystem::readFileTextUTF8(resolvedPath);
	if (!fileContentOpt) {
		lua_pushfstring(this->state_, "cannot read file '%s'", path.c_str());
		return LUA_ERRFILE;
	}

	const std::string chunkName = "@" + resolvedPath.string();
	const int status = luaL_loadbufferx(this->state_, fileContentOpt->data(), fileContentOpt->size(), chunkName.c_str(), "t");
	if (status != LUA_OK)
		return status;

	lua_getfield(this->state_, LUA_REGISTRYINDEX, this->envKey_.c_str());
	lua_setfenv(this->state_, -2);

	return LUA_OK;
}

bool LuaVM::doFile(const std::string& path) {
	if (!this->isInitialized()) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Lua state is not initialized. Cannot dofile %s", path.c_str());
		return false;
	}

	if (this->loadFile(path) != LUA_OK) {
		this->reportLoadError();
		return false;
	}

	return this->protectedCall(0, 0);
}

[[nodiscard]] bool LuaVM::getField(const std::string& fieldName) {
	lua_State* L = this->state_;
	if (!this->isInitialized()) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Lua state is not initialized. Cannot get field %s", fieldName.c_str());

		lua_pushnil(L);
		return false;
	}

	lua_getfield(L, LUA_REGISTRYINDEX, this->envKey_.c_str());
	lua_getfield(L, -1, fieldName.c_str());
	lua_remove(L, -2);

	return !lua_isnil(L, -1);
}

bool LuaVM::protectedCall(int argCount, int resultCount) {
	lua_State* L = this->state_;
	if (!this->isInitialized()) {
		lua_pop(L, argCount + 1);

		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Lua state is not initialized. Cannot make a protected call.");
		return false;
	}

	const int handlerIndex = lua_gettop(L) - argCount;
	lua_rawgeti(L, LUA_REGISTRYINDEX, this->errorHandlerRef_);
	lua_insert(L, handlerIndex);

	this->pendingError_.reset();

	LuaVM* previousVM = LuaUtils::getLuaVM(L);
	LuaUtils::setLuaVM(L, this);

	int status = lua_pcall(L, argCount, resultCount, handlerIndex);

	LuaUtils::setLuaVM(L, previousVM);

	lua_remove(L, handlerIndex);
	if (status == LUA_OK)
		return true;

	const std::string rawMessage = popString(L, LuaError::kUnknownMessage);
	if (this->pendingError_.frames.empty())
		this->pendingError_.message = rawMessage;

	this->reportError(this->pendingError_);
	return false;
}

bool LuaVM::callFunction(const std::string& functionName, int argCount, int resultCount) {
	lua_State* L = this->state_;

	if (!this->isInitialized()) {
		lua_pop(L, argCount);

		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Lua state is not initialized. Cannot call function %s", functionName.c_str());
		return false;
	}

	if(!this->getField(functionName)) {
		lua_pop(L, argCount);

		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Function %s does not exist in the LuaVM.", functionName.c_str());
		return false;
	}

	if (!lua_isfunction(L, -1)) {
		lua_pop(L, argCount + 1);

		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Field %s is not a function in the LuaVM.", functionName.c_str());
		return false;
	}

	lua_insert(L, -(argCount + 1));
	return this->protectedCall(argCount, resultCount);
}