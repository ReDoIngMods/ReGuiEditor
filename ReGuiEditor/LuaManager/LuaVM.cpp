#include "LuaVM.hpp"

#include "LuaError.hpp"

#include <FilesystemManager/FilesystemManager.hpp>

#include <string>
#include <utility>
#include <vector>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

constexpr const char* VM_REGISTRY_KEY = "VMPtr";
constexpr float SCRIPT_POLL_INTERVAL = 0.5f;

// Keep the VM alive after a failed entry script so the watcher can hot-reload a fixed one.
#if defined(_DEBUG) || defined(DEBUG)
	constexpr bool KEEP_ALIVE_ON_ENTRY_FAILURE = true;
#else
	constexpr bool KEEP_ALIVE_ON_ENTRY_FAILURE = false;
#endif

inline static std::string popString(lua_State* L, const char* fallback = "") {
	const char* value = lua_tostring(L, -1);
	std::string result = value ? value : fallback;

	lua_pop(L, 1);

	return result;
}

inline static std::string getChunkName(const std::string& path) {
	return "@" + path;
}

class ActiveVmScope {
	lua_State* state_ = nullptr;
	LuaVM* previous_ = nullptr;

	void setLuaVM(LuaVM* vm) const {
		lua_pushlightuserdata(this->state_, vm);
		lua_setfield(this->state_, LUA_REGISTRYINDEX, VM_REGISTRY_KEY);
	}
public:
	ActiveVmScope(lua_State* state, LuaVM* vm) : state_(state), previous_(LuaVM::fromState(state)) { this->setLuaVM(vm); }
	~ActiveVmScope() { this->setLuaVM(this->previous_); }

	ActiveVmScope(const ActiveVmScope&) = delete;
	ActiveVmScope& operator=(const ActiveVmScope&) = delete;
};

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
	LuaVM* self = LuaVM::fromState(L);
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

LuaVM* LuaVM::fromState(lua_State* L) {
	lua_getfield(L, LUA_REGISTRYINDEX, VM_REGISTRY_KEY);
	LuaVM* vm = static_cast<LuaVM*>(lua_touserdata(L, -1));
	lua_pop(L, 1);

	return vm;
}

void LuaVM::dumpStack(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "--- LUA STACK DUMP ---");

	for (int index = 1, top = lua_gettop(L); index <= top; ++index) {
		switch (lua_type(L, index)) {
		case LUA_TSTRING:
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %s", index, lua_tostring(L, index));
			break;
		case LUA_TNUMBER:
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %g", index, lua_tonumber(L, index));
			break;
		case LUA_TBOOLEAN:
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %s", index, lua_toboolean(L, index) ? "true" : "false");
			break;
		default:
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %s", index, luaL_typename(L, index));
			break;
		}
	}
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

	luaL_unref(L, LUA_REGISTRYINDEX, this->errorHandlerRef_);
	this->errorHandlerRef_ = LUA_NOREF;

	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, this->envKey_.c_str());
	lua_gc(L, LUA_GCCOLLECT, 0);

	this->initialized_ = false;
	this->scriptWatcher_.clearFiles();
	this->chunkPaths_.clear();
	this->pollTimer_ = 0.0f;
}

bool LuaVM::initialize(const std::string& entryScript) {
	if (this->isInitialized())
		return true;

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Initializing Lua VM for mod %s...", this->modLocalId_.c_str());

	this->createEnvironment();
	if (this->doFile(entryScript) || KEEP_ALIVE_ON_ENTRY_FAILURE)
		return true;

	this->cleanup();
	return false;
}

void LuaVM::cleanup() {
	if (!this->isInitialized())
		return;

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Cleaning up Lua VM for mod %s...", this->modLocalId_.c_str());

	this->destroyEnvironment();
}

void LuaVM::update(float deltaTime) {
	if (!this->isInitialized())
		return;

	this->updateScriptWatcher(deltaTime);

	if (this->pushHook("OnUpdate")) {
		lua_pushnumber(this->state_, deltaTime);
		this->protectedCall(1, 0);
	}
}

void LuaVM::tick() {
	if (this->pushHook("OnTick"))
		this->protectedCall(0, 0);
}

bool LuaVM::pushHook(const char* name) {
	if (!this->isInitialized())
		return false;

	this->getField(name);
	if (lua_isfunction(this->state_, -1))
		return true;

	lua_pop(this->state_, 1);
	return false;
}

void LuaVM::updateScriptWatcher(float deltaTime) {
	this->pollTimer_ += deltaTime;
	if (this->pollTimer_ < SCRIPT_POLL_INTERVAL)
		return;

	this->pollTimer_ = 0.0f;

	for (const std::string& path : this->scriptWatcher_.collectChangedFiles())
		this->doFile(path);
}

bool LuaVM::findChunkPath(const std::string& chunkName, std::string& path) const {
	const auto iterator = this->chunkPaths_.find(chunkName);
	if (iterator == this->chunkPaths_.end() || iterator->second.empty())
		return false;

	path = iterator->second;
	return true;
}

int LuaVM::loadFile(const std::string& path) {
	lua_State* L = this->state_;
	FilesystemManager& filesystemManager = FilesystemManager::getInstance();

	std::string resolvedPath = path;
	if (!filesystemManager.resolveContentData(resolvedPath, this->modLocalId_.c_str())) {
		lua_pushfstring(L, "cannot resolve path '%s'", path.c_str());
		return LUA_ERRFILE;
	}

	this->scriptWatcher_.watchFile(resolvedPath);

	std::vector<char> content;
	if (!filesystemManager.readFile(resolvedPath, content)) {
		lua_pushfstring(L, "cannot read file '%s'", path.c_str());
		return LUA_ERRFILE;
	}

	const std::string chunkName = getChunkName(resolvedPath);

	auto [chunkPath, inserted] = this->chunkPaths_.try_emplace(chunkName, resolvedPath);
	if (!inserted && chunkPath->second != resolvedPath)
		chunkPath->second.clear();

	const int status = luaL_loadbufferx(L, content.data(), content.size(), chunkName.c_str(), "t");
	if (status != LUA_OK)
		return status;

	lua_getfield(L, LUA_REGISTRYINDEX, this->envKey_.c_str());
	lua_setfenv(L, -2);

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

void LuaVM::getField(const std::string& fieldName) {
	lua_State* L = this->state_;

	if (!this->isInitialized()) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Lua state is not initialized. Cannot get field %s", fieldName.c_str());

		lua_pushnil(L);
		return;
	}

	lua_getfield(L, LUA_REGISTRYINDEX, this->envKey_.c_str());
	lua_getfield(L, -1, fieldName.c_str());
	lua_remove(L, -2);
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

	int status;
	{
		const ActiveVmScope scope(L, this);
		status = lua_pcall(L, argCount, resultCount, handlerIndex);
	}

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

	this->getField(functionName);
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, argCount + 1);

		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Function %s does not exist or is not a function in the LuaVM.", functionName.c_str());
		return false;
	}

	lua_insert(L, -(argCount + 1));
	return this->protectedCall(argCount, resultCount);
}