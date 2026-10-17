#pragma once
#include "LuaError.hpp"
#include "LuaScriptWatcher.hpp"

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <string>
#include <unordered_map>

class LuaVM {
	lua_State* state_ = nullptr;
	std::string modLocalId_;
	std::string envKey_;
	bool initialized_ = false;

	int errorHandlerRef_ = LUA_NOREF;

	LuaScriptWatcher scriptWatcher_;
	std::unordered_map<std::string, std::string> chunkPaths_;
	float pollTimer_ = 0.0f;
	LuaError pendingError_;

	void createEnvironment();
	void destroyEnvironment();
	void updateScriptWatcher(float deltaTime);

	bool pushHook(const char* name);

	void reportLoadError();
	void reportError(const LuaError& error);

	static int errorHandler(lua_State* L);
public:
	LuaVM(lua_State* state, const std::string& modLocalId) : state_(state), modLocalId_(modLocalId), envKey_("env." + modLocalId) {}
	~LuaVM() { this->cleanup(); }

	LuaVM(const LuaVM&) = delete;
	LuaVM& operator=(const LuaVM&) = delete;
	LuaVM(LuaVM&&) = delete;
	LuaVM& operator=(LuaVM&&) = delete;

	static LuaVM* fromState(lua_State* L);
	static void dumpStack(lua_State* L);

	bool initialize(const std::string& entryScript);
	void cleanup();

	void update(float deltaTime);
	void tick();

	lua_State* getState() const { return this->state_; }
	bool isInitialized() const { return this->initialized_; }
	const std::string& getModLocalId() const { return this->modLocalId_; }

	[[nodiscard]] bool findChunkPath(const std::string& chunkName, std::string& path) const;

	int loadFile(const std::string& path);
	bool doFile(const std::string& path);

	void getField(const std::string& fieldName);
	bool protectedCall(int argCount, int resultCount);
	bool callFunction(const std::string& functionName, int argCount, int resultCount);
};