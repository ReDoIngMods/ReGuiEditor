#pragma once
#include "LuaClassInstance.hpp"
#include "LuaError.hpp"
#include "LuaScriptWatcher.hpp"

#include <UTILS/Uuid.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <memory>
#include <string>
#include <vector>

class LuaVM {
	lua_State* state_ = nullptr;
	UTILS::UUID modLocalId_;
	std::string envKey_;
	std::vector<std::unique_ptr<LuaClassInstance>> classInstances_;
	bool initialized_ = false;

	int errorHandlerRef_ = LUA_NOREF;

	LuaScriptWatcher scriptWatcher_;
	float pollTimer_ = 0.0f;
	LuaError pendingError_;

	void createEnvironment();
	void destroyEnvironment();
	void updateScriptWatcher(float deltaTime);

	void reportLoadError();
	void reportError(const LuaError& error);

	static int errorHandler(lua_State* L);
public:
	LuaVM(lua_State* state, const UTILS::UUID& modLocalId) : state_(state), modLocalId_(modLocalId), envKey_("env." + modLocalId.toAStr()) {}
	~LuaVM() { this->cleanup(); }

	LuaVM(LuaVM&) = delete;
	LuaVM(LuaVM&&) = delete;
	LuaVM& operator=(LuaVM&) = delete;
	LuaVM& operator=(LuaVM&&) = delete;

	[[nodiscard]] bool initialize();
	[[nodiscard]] LuaClassInstance* loadClass(LuaClassType type, const std::string& scriptPath, const std::string& className);
	void cleanup();

	void update(float deltaTime);

	[[nodiscard]] lua_State* getState() const { return this->state_; }
	[[nodiscard]] bool isInitialized() const { return this->initialized_; }
	[[nodiscard]] const UTILS::UUID& getModLocalId() const { return this->modLocalId_; }
	[[nodiscard]] const std::vector<std::unique_ptr<LuaClassInstance>>& getClassInstances() const { return this->classInstances_; }

	[[nodiscard]] int loadFile(const std::string& path);
	[[nodiscard]] bool doFile(const std::string& path);

	[[nodiscard]] bool getField(const std::string& fieldName);
	[[nodiscard]] bool protectedCall(int argCount, int resultCount);
	[[nodiscard]] bool callFunction(const std::string& functionName, int argCount, int resultCount);
};