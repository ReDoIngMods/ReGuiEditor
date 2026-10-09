#include "LuaVM.hpp"

#include <FilesystemManager/FilesystemManager.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

inline static std::string popString(lua_State* L, const char* defaultValue = "") {
	const char* value = lua_tostring(L, -1);
	std::string result = value ? value : defaultValue;

	lua_pop(L, 1);

	return result;
}

inline static std::string getChunkName(const std::string& path) {
	std::string name = path;
	if (name.size() > 60) {
		const size_t excess = name.size() - 60;
		const size_t dotCount = std::min<size_t>(3, excess);
		name = std::string(dotCount, '.') + name.substr(excess);
	}

	return "@" + name;
}

inline static void setCurrentVm(lua_State* L, LuaVM* vm) {
	if (vm)
		lua_pushlightuserdata(L, vm);
	else
		lua_pushnil(L);

	lua_setfield(L, LUA_REGISTRYINDEX, "VMPtr");
}

LuaVM* LuaVM::fromState(lua_State* L) {
	lua_getfield(L, LUA_REGISTRYINDEX, "VMPtr");
	LuaVM* vm = static_cast<LuaVM*>(lua_touserdata(L, -1));
	lua_pop(L, 1);

	return vm;
}

bool LuaVM::initialize(const std::string& entryScript) {
	if (this->isInitialized())
		return true;

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Initializing Lua VM for mod %s...", this->modLocalId_.c_str());

	lua_State* L = this->state_;
	lua_newtable(L);

	lua_newtable(L);
	lua_getglobal(L, "unsafe_env");
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);

	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "_G");

	lua_setfield(L, LUA_REGISTRYINDEX, this->envKey_.c_str());
	this->initialized_ = true;

	return this->doFile(entryScript);
}

void LuaVM::cleanup() {
	if (!this->isInitialized())
		return;

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Cleaning up Lua VM for mod %s...", this->modLocalId_.c_str());

	lua_pushnil(this->state_);
	lua_setfield(this->state_, LUA_REGISTRYINDEX, this->envKey_.c_str());
	lua_gc(this->state_, LUA_GCCOLLECT, 0);

	this->initialized_ = false;
	this->scriptWatcher_.clearFiles();
}

void LuaVM::update(float deltaTime) {
	if (!this->isInitialized())
		return;

	this->pollTimer_ += deltaTime;
	if (this->pollTimer_ < 0.5f)
		return;

	this->pollTimer_ = 0.0f;

	for (const std::string& path : this->scriptWatcher_.collectChangedFiles())
		this->doFile(path);
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

void LuaVM::reportLoadError() {
	LuaError error;
	error.message = popString(this->state_, "unknown error");

	this->reportError(error);
}

void LuaVM::reportError(const LuaError& error) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "ERROR: %s", error.message.c_str());
	if (error.frames.empty())
		return;

	std::stringstream ss;
	ss << "--- Lua Stack Traceback ---";

	for (const LuaStackFrame& frame : error.frames) {
		ss << "\n\t" << frame.source;
		if (frame.line != -1)
			ss << ":" << frame.line << ":";

		if (!frame.function.empty())
			ss << " in " << frame.function;
	}

	CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "%s", ss.str().c_str());
}

int LuaVM::loadFile(const std::string& path) {
	FilesystemManager& filesystemManager = FilesystemManager::getInstance();

	std::string resolvedPath = path;
	if (!filesystemManager.resolveContentData(resolvedPath, this->modLocalId_.c_str())) {
		lua_pushfstring(this->state_, "cannot resolve path '%s'", path.c_str());
		return LUA_ERRFILE;
	}

	this->scriptWatcher_.watchFile(resolvedPath);

	std::vector<char> content;
	if (!filesystemManager.readFile(resolvedPath, content)) {
		lua_pushfstring(this->state_, "cannot read file '%s'", path.c_str());
		return LUA_ERRFILE;
	}

	const std::string chunkName = getChunkName(resolvedPath);
	const int status = luaL_loadbuffer(this->state_, content.data(), content.size(), chunkName.c_str());
	if (status != LUA_OK)
		return status;

	lua_getfield(this->state_, LUA_REGISTRYINDEX, this->envKey_.c_str());
	lua_setfenv(this->state_, -2);

	return LUA_OK;
}

bool LuaVM::protectedCall(int argCount, int resultCount) {
	lua_State* L = this->state_;

	const int handlerIndex = lua_gettop(L) - argCount;
	lua_pushcfunction(L, LuaVM::errorHandler);
	lua_insert(L, handlerIndex);

	this->pendingError_.reset();

	LuaVM* previousVM = LuaVM::fromState(L);
	lua_pushlightuserdata(L, this);
	lua_setfield(L, LUA_REGISTRYINDEX, "VMPtr");

	const int status = lua_pcall(L, argCount, resultCount, handlerIndex);
	lua_remove(L, handlerIndex);

	lua_pushlightuserdata(L, previousVM);
	lua_setfield(L, LUA_REGISTRYINDEX, "VMPtr");

	if (status != LUA_OK) {
		const std::string message = popString(L, "unknown error");
		if (this->pendingError_.message.empty())
			this->pendingError_.message = message;

		this->reportError(this->pendingError_);
		return false;
	}

	return true;
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
		default:
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %s", index, luaL_typename(L, index));
			break;
		}
	}
}