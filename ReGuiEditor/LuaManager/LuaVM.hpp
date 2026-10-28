#pragma once
#include "LuaError.hpp"
#include "LuaScriptWatcher.hpp"

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <string>

class LuaVM {
	lua_State* state_ = nullptr;
	std::string modLocalId_;
	std::string envKey_;
	bool initialized_ = false;

	int errorHandlerRef_ = LUA_NOREF;

	LuaScriptWatcher scriptWatcher_;
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

	LuaVM(LuaVM&) = delete;
	LuaVM(LuaVM&&) = delete;
	LuaVM& operator=(LuaVM&) = delete;
	LuaVM& operator=(LuaVM&&) = delete;

	[[nodiscard]] bool initialize(const std::string& entryScript);
	void cleanup();

	void update(float deltaTime);
	void tick();

	[[nodiscard]] lua_State* getState() const { return this->state_; }
	[[nodiscard]] bool isInitialized() const { return this->initialized_; }
	[[nodiscard]] const std::string& getModLocalId() const { return this->modLocalId_; }

	[[nodiscard]] int loadFile(const std::string& path);
	[[nodiscard]] bool doFile(const std::string& path);

	[[nodiscard]] bool getField(const std::string& fieldName);
	[[nodiscard]] bool protectedCall(int argCount, int resultCount);
	[[nodiscard]] bool callFunction(const std::string& functionName, int argCount, int resultCount);
};