#pragma once
#include "LuaScriptWatcher.hpp"

#include <LuaJIT/lua.h>

#include <string>
#include <unordered_map>
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

	void reset() {
		this->message = "unknown error";
		this->frames.clear();
	}

	void setMessage(const char* message, const char* defaultMessage = nullptr) {
		this->message = message ? message : (defaultMessage ? defaultMessage : "unknown error");
	}
};

class LuaVM {
	lua_State* state_ = nullptr;
	std::string modLocalId_;
	std::string envKey_;
	bool initialized_ = false;

	LuaScriptWatcher scriptWatcher_;

	// Chunk names are shortened (and so not unique), this maps them back to the file they came from.
	// A chunk name shared by two different files maps to an empty path.
	std::unordered_map<std::string, std::string> chunkPaths_;
	float pollTimer_ = 0.0f;
	LuaError pendingError_;

	bool protectedCall(int argCount, int resultCount);

	void reportLoadError();
	void reportError(const LuaError& error);

	static int errorHandler(lua_State* L);
public:
	LuaVM(lua_State* state, const std::string& modLocalId) : state_(state), modLocalId_(modLocalId), envKey_("env." + modLocalId) { }
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

	lua_State* getState() const { return this->state_; }
	bool isInitialized() const { return this->initialized_; }
	const std::string& getModLocalId() const { return this->modLocalId_; }

	// Full "$KEY/..." path of the file a chunk name belongs to. False when unknown or ambiguous.
	[[nodiscard]] bool findChunkPath(const std::string& chunkName, std::string& path) const;

	int loadFile(const std::string& path);

	bool doFile(const std::string& path);
};