#include "LuaManager.hpp"

#include <FilesystemManager/FilesystemManager.hpp>
#include <UTILS/Console.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>
#include <LuaJIT/lualib.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

constexpr const char* MOD_LOCALID = "0e0779e3-a4ae-46c7-89db-1596a935180d";
constexpr const char* ENTRY_SCRIPT_PATH = "$CONTENT_0e0779e3-a4ae-46c7-89db-1596a935180d/Scripts/Application/Loader.lua";

constexpr const char* SCRIPT_ENV_KEY = "reguieditor.scriptEnvironment";

constexpr float POLL_INTERVAL = 0.5f;

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

static int luaPrint(lua_State* L) {
	const int argumentCount = lua_gettop(L);
	std::string output;

	for (int index = 1; index <= argumentCount; ++index) {
		lua_getglobal(L, "tostring");
		lua_pushvalue(L, index);
		lua_call(L, 1, 1);

		size_t length;
		const char* str = lua_tolstring(L, -1, &length);
		if (!str)
			return luaL_error(L, "'tostring' must return a string to 'print'");

		if (index > 1)
			output += '\t';

		output.append(str, length);
		lua_pop(L, 1);
	}

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "%.*s", static_cast<int>(output.size()), output.c_str());
	return 0;
}

static int luaDofile(lua_State* L) {
	const char* path = luaL_checkstring(L, 1);
	lua_settop(L, 1);

	if (LuaManager::getInstance().loadFile(path) != LUA_OK)
		return lua_error(L);

	lua_call(L, 0, LUA_MULTRET);
	return lua_gettop(L) - 1;
}

static int luaPanic(lua_State* L) {
	const char* message = lua_tostring(L, 1);
	if (!message)
		message = "unknown error";

	CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "PANIC: %s", message);
	LuaManager::getInstance().dumpStack(L);
	
	return 0;
}

const luaL_Reg globalFunctions[] = {
	{"print", luaPrint},
	{"dofile", luaDofile},
	{nullptr, nullptr},
};

bool LuaManager::initialize() {
	if (this->luaState_)
		return true;

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Initializing LuaManager...");

	this->luaState_ = luaL_newstate();
	if (!this->luaState_) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create Lua state.");
		return false;
	}

	luaL_openlibs(this->luaState_);

	lua_pushvalue(this->luaState_, LUA_GLOBALSINDEX);
	luaL_register(this->luaState_, nullptr, globalFunctions);
	lua_pop(this->luaState_, 1);

	lua_newtable(this->luaState_);
	lua_setglobal(this->luaState_, "sm");

	lua_newtable(this->luaState_);
	lua_setglobal(this->luaState_, "reguieditor");

	for (const LuaManager::Library& library : this->libraries_)
		this->openLibrary(library);

	this->createScriptEnvironment();
	
	if (this->doFile(ENTRY_SCRIPT_PATH) != LUA_OK) {
		this->reportLoadError();
		return false;
	}

	return true;
}

void LuaManager::createScriptEnvironment() {
	lua_State* L = this->luaState_;

	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "_G");

	lua_newtable(L);
	lua_pushvalue(L, LUA_GLOBALSINDEX);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);

	lua_setfield(L, LUA_REGISTRYINDEX, SCRIPT_ENV_KEY);
}

void LuaManager::cleanup() {
	if (!this->luaState_)
		return;

	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Cleaning up LuaManager...");

	lua_close(this->luaState_);

	this->luaState_ = nullptr;
	this->scriptWatcher_.clearFiles();
}

void LuaManager::update(float deltaTime) {
	if (!this->luaState_)
		return;

	this->pollTimer_ += deltaTime;
	if (this->pollTimer_ < POLL_INTERVAL)
		return;

	this->pollTimer_ = 0.0f;

	for (const std::string& path : this->scriptWatcher_.collectChangedFiles())
		this->doFile(path);
}

int LuaManager::errorHandler(lua_State* L) {
	LuaError& luaError = LuaManager::getInstance().pendingError_;

	const char* message = lua_tostring(L, 1);
	luaError.message = message ? message : luaL_typename(L, 1);
	luaError.frames.clear();

	lua_Debug debug;
	for (int level = 1; lua_getstack(L, level, &debug); ++level) {
		lua_getinfo(L, "nSl", &debug);

		LuaStackFrame frame;
		frame.source = debug.short_src;
		frame.function = debug.name ? debug.name : "";
		frame.what = debug.what;
		frame.line = debug.currentline;

		luaError.frames.push_back(frame);
	}

	return 1;
}

void LuaManager::reportLoadError() {
	LuaError error;
	error.message = popString(this->luaState_, "unknown error");

	this->reportError(error);
}

void LuaManager::reportError(const LuaError& error) {
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

int LuaManager::loadFile(const std::string& path) {
	FilesystemManager& filesystemManager = FilesystemManager::getInstance();

	std::string resolvedPath = path;
	if (!filesystemManager.resolveContentData(resolvedPath, MOD_LOCALID)) {
		lua_pushfstring(this->luaState_, "cannot resolve path '%s'", path.c_str());
		return LUA_ERRFILE;
	}

	this->scriptWatcher_.watchFile(resolvedPath);

	std::vector<char> content;
	if (!filesystemManager.readFile(resolvedPath, content)) {
		lua_pushfstring(this->luaState_, "cannot read file '%s'", path.c_str());
		return LUA_ERRFILE;
	}

	const std::string chunkName = getChunkName(resolvedPath);
	const int status = luaL_loadbuffer(this->luaState_, content.data(), content.size(), chunkName.c_str());
	if (status == LUA_OK) {
		lua_getfield(this->luaState_, LUA_REGISTRYINDEX, SCRIPT_ENV_KEY);
		lua_setfenv(this->luaState_, -2);
	}

	return status;
}

bool LuaManager::protectedCall(int argCount, int resultCount) {
	lua_State* L = this->luaState_;

	const int handlerIndex = lua_gettop(L) - argCount;
	lua_pushcfunction(L, LuaManager::errorHandler);
	lua_insert(L, handlerIndex);

	this->pendingError_ = {};

	const int status = lua_pcall(L, argCount, resultCount, handlerIndex);
	lua_remove(L, handlerIndex);

	if (status != LUA_OK) {
		const std::string message = popString(L, "unknown error");
		if (this->pendingError_.message.empty())
			this->pendingError_.message = message;

		this->reportError(this->pendingError_);
		return false;
	}

	return true;
}

bool LuaManager::doFile(const std::string& path) {
	if (!this->luaState_) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Lua state is not initialized. Cannot dofile %s", path.c_str());
		return false;
	}

	if (this->loadFile(path) != LUA_OK) {
		this->reportLoadError();
		return false;
	}

	return this->protectedCall(0, 0);
}

bool LuaManager::doString(const std::string& code, const char* chunkName) {
	if (!this->luaState_) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Lua state is not initialized. Cannot dostring.");
		return false;
	}

	if (luaL_loadbuffer(this->luaState_, code.data(), code.size(), chunkName) != LUA_OK) {
		this->reportLoadError();
		return false;
	}

	return this->protectedCall(0, 0);
}

void LuaManager::openLibrary(const Library& library) {
	luaL_register(this->luaState_, library.name, library.functions);
	lua_pop(this->luaState_, 1);
}

void LuaManager::registerLibrary(const char* name, const luaL_Reg* functions) {
	const Library& library = this->libraries_.emplace_back(Library{ name, functions });

	if (this->luaState_)
		this->openLibrary(library);
}

void LuaManager::dumpStack(lua_State* L) const {
	CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "--- LUA STACK DUMP ---");

	for (int index = 1, top = lua_gettop(L); index <= top; ++index) {
		if (lua_type(L, index) == LUA_TSTRING || lua_type(L, index) == LUA_TNUMBER) {
			size_t length;

			const char* str = lua_tolstring(L, index, &length);
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %.*s", index, static_cast<int>(length), str);
		} else {
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %s", index, luaL_typename(L, index));
		}
	}
}