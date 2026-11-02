#include "LuaState.hpp"

#include "LuaScriptError.hpp"

#include <Filesystem/DirectoryManager.hpp>
#include <Filesystem/FilesystemManager.hpp>

#include <Modules/Misc/Strings.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>
#include <LuaJIT/lualib.h>

#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <utility>
#include <vector>

static constexpr const char* CALLABLE_METATABLE = "ReGuiEditor.Callable";

static int luaPanic(lua_State* L) {
	const char* message = lua_tostring(L, 1);
	CONSOLE_ERROR("PANIC: %s", message ? message : "unknown error");

	return 0;
}

static int callableGarbageCollector(lua_State* L) {
	std::destroy_at(static_cast<ReGuiEditor::LuaCallable*>(lua_touserdata(L, 1)));
	return 0;
}

static int callableInvoke(lua_State* L) {
	auto& callable = *static_cast<ReGuiEditor::LuaCallable*>(lua_touserdata(L, lua_upvalueindex(1)));
	try {
		return callable(L);
	} catch (const std::exception& e) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "C++ Exception: %s", e.what());
	}

	lua_pushliteral(L, "C++ Exception");
	return lua_error(L);
}

static std::string popString(lua_State* L, int index = -1, const char* fallback = "") {
	size_t length = 0;
	const char* data = lua_tolstring(L, index, &length);
	if (!data) {
		lua_pop(L, 1);
		return fallback;
	}

	const std::string result(data, length);
	lua_pop(L, 1);

	return result;
}

static std::string describeErrorObject(lua_State* L, int index) {
	const int type = lua_type(L, index);
	if (type == LUA_TSTRING || type == LUA_TNUMBER)
		return lua_tostring(L, index);

	if (!luaL_callmeta(L, index, "__tostring"))
		return ReGuiEditor::formatString("error object is a %s value", luaL_typename(L, index));

	if (lua_type(L, -1) != LUA_TSTRING)
		return ReGuiEditor::formatString("error object is a %s value (metamethod __tostring did not return a string)", luaL_typename(L, index));

	return popString(L, -1, "Unknown error");
}

static int errorHandler(lua_State* L) {
	ReGuiEditor::LuaScriptError* error = static_cast<ReGuiEditor::LuaScriptError*>(lua_touserdata(L, lua_upvalueindex(1)));
	error->message = describeErrorObject(L, 1);

	lua_Debug debug;
	for (int level = 1; lua_getstack(L, level, &debug); ++level) {
		lua_getinfo(L, "nSl", &debug);

		ReGuiEditor::LuaStackFrame frame;
		frame.source = debug.short_src;
		frame.functionName = debug.name ? debug.name : "";
		frame.what = debug.what;
		frame.line = debug.currentline;

		error->frames.push_back(std::move(frame));
	}

	lua_pushvalue(L, 1);
	return 1;
}

ReGuiEditor::LuaState::~LuaState() {
	if (this->isOwner_ && this->state_) {
		lua_close(this->state_);
		this->state_ = nullptr;
	}
}

bool ReGuiEditor::LuaState::initialize() {
	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Initializing Lua state...");

	this->state_ = luaL_newstate();
	if (!this->state_) {
		CONSOLE_ERROR("Failed to create Lua state.");
		return false;
	}

	this->isOwner_ = true;

	struct LuaLib {
		const char* name;
		lua_CFunction openFunc;
	};

	constexpr LuaLib LUA_LIBS[] = {
		{ "",        luaopen_base   },
		{ "package", luaopen_package},
		{ "table",   luaopen_table  },
		{ "io",      luaopen_io     },
		{ "os",      luaopen_os     },
		{ "string",  luaopen_string },
		{ "math",    luaopen_math   },
		{ "debug",   luaopen_debug  },
		{ "bit",     luaopen_bit    },
		{ "jit",     luaopen_jit    },
		{ "ffi",     luaopen_ffi    },
	};

	for (const auto& luaLib : LUA_LIBS) {
		lua_pushcfunction(this->state_, luaLib.openFunc);
		lua_pushstring(this->state_, luaLib.name);
		lua_call(this->state_, 1, 0);

		CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Loaded Lua library: %s", strcmp(luaLib.name, "") == 0 ? "base" : luaLib.name);
	}

	return true;
}

ReGuiEditor::LuaResult ReGuiEditor::LuaState::loadFile(const std::string& path, const ReGuiEditor::Uuid& modLocalId, int environmentRef) {
	std::string resolvedPath = path;
	if (!ReGuiEditor::DirectoryManager::getInstance().resolveContentData(resolvedPath, modLocalId))
		return LuaResult::generateFailure("cannot resolve path '%s'", path.c_str());

	std::vector<char> content;
	if (!ReGuiEditor::FilesystemManager::getInstance().readFile(resolvedPath, content))
		return LuaResult::generateFailure("cannot read file '%s'", resolvedPath.c_str());

	const std::string chunkName = "@" + resolvedPath;
	if (luaL_loadbufferx(this->state_, content.data(), content.size(), chunkName.c_str(), "t") != LUA_OK)
		return LuaResult::generateFailure(popString(this->state_));

	lua_rawgeti(this->state_, LUA_REGISTRYINDEX, environmentRef);
	lua_setfenv(this->state_, -2);

	return LuaResult::generateSuccess();
}

ReGuiEditor::LuaResult ReGuiEditor::LuaState::doFile(const std::string& path, const ReGuiEditor::Uuid& modLocalId, int environmentRef) {
	LuaResult loaded = this->loadFile(path, modLocalId, environmentRef);
	if (!loaded)
		return loaded;

	return this->call(0, 0);
}

ReGuiEditor::LuaResult ReGuiEditor::LuaState::call(int argCount, int resultCount) {
	const int base = lua_gettop(this->state_) - argCount - 1;

	LuaScriptError error;
	lua_pushlightuserdata(this->state_, &error);
	lua_pushcclosure(this->state_, errorHandler, 1);
	lua_insert(this->state_, base + 1);

	if (lua_pcall(this->state_, argCount, resultCount, base + 1) != LUA_OK) {
		std::string rawMessage = popString(this->state_, -1);
		if (error.message.empty())
			error.message = std::move(rawMessage);

		lua_settop(this->state_, base);

		return LuaResult::generateFailure(std::move(error));
	}

	lua_remove(this->state_, base + 1);

	return LuaResult::generateSuccess();
}

void ReGuiEditor::LuaState::pushCallable(LuaCallable callable) {
	void* memory = lua_newuserdata(this->state_, sizeof(LuaCallable));
	new (memory) LuaCallable(std::move(callable));

	if (luaL_newmetatable(this->state_, CALLABLE_METATABLE)) {
		lua_pushcfunction(this->state_, callableGarbageCollector);
		lua_setfield(this->state_, -2, "__gc");
	}
	lua_setmetatable(this->state_, -2);
	lua_pushcclosure(this->state_, callableInvoke, 1);
}
