#pragma once
#include "LuaScriptWatcher.hpp"

#include <UTILS/Singleton.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <string>
#include <vector>

struct LuaStackFrame {
	std::string source;   // Shortened chunk name, e.g. "...Application/Loader.lua" or "[C]".
	std::string function; // Empty when Lua does not know the name.
	std::string what;     // "Lua", "C", "main" or "tail".
	int line = -1;        // -1 when there is no line information (C functions).
};

struct LuaError {
	std::string message;
	std::vector<LuaStackFrame> frames;
};

class LuaManager : public UTILS::Singleton<LuaManager> {
	struct Library {
		const char* name;
		const luaL_Reg* functions;
	};

	lua_State* luaState_ = nullptr;

	std::vector<Library> libraries_;
	LuaScriptWatcher scriptWatcher_;
	float pollTimer_ = 0.0f;
	LuaError pendingError_;

	bool protectedCall(int argCount, int resultCount);
	void openLibrary(const Library& library);
	void createScriptEnvironment();

	void reportLoadError();
	void reportError(const LuaError& error);

	static int errorHandler(lua_State* L);
public:
	LuaManager() = default;
	~LuaManager() { this->cleanup(); }

	LuaManager(const LuaManager&) = delete;
	LuaManager& operator=(const LuaManager&) = delete;
	LuaManager(LuaManager&&) = delete;
	LuaManager& operator=(LuaManager&&) = delete;

	bool initialize();
	void cleanup();

	void update(float deltaTime);

	lua_State* getLuaState() const { return this->luaState_; }

	int loadFile(const std::string& path);

	bool doFile(const std::string& path);
	bool doString(const std::string& code, const char* chunkName = "=<string>");

	void registerLibrary(const char* name, const luaL_Reg* functions);

	void dumpStack(lua_State* L) const;
};