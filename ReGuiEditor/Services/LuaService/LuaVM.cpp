#include "LuaVM.hpp"

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include "LuaResult.hpp"
#include <optional>
#include <string>

constexpr const char* ACTIVE_VM_KEY = "ReGuiEditor.ActiveVM";
constexpr const char* BASE_ENVIRONMENT_GLOBAL = "unsafe_env";

bool ReGuiEditor::LuaVM::initialize() {
	const std::string localId = this->modLocalId_.toString();
	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Initializing Lua VM for mod %s...", localId.c_str());

	lua_State* L = this->luaState_.getState();

	lua_getglobal(L, BASE_ENVIRONMENT_GLOBAL);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);

		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Cannot create environment for mod %s: global '%s' is missing.", localId.c_str(), BASE_ENVIRONMENT_GLOBAL);
		return false;
	}

	lua_newtable(L);	// env = {}
	lua_newtable(L);	// metatable = {}

	lua_pushvalue(L, -3);
	lua_setfield(L, -2, "__index");	// metatable.__index = unsafe_env

	lua_setmetatable(L, -2);	// setmetatable(env, metatable)

	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "_G");	// env._G = env

	this->environmentRef_ = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_pop(L, 1);	// unsafe_env

	return true;
}

void ReGuiEditor::LuaVM::shutdown() {
	CONSOLE_LOG(LEVEL_LUA, COLOR_GRAY, "Cleaning up Lua VM for mod %s...", this->modLocalId_.toString().c_str());

	lua_State* L = this->luaState_.getState();
	luaL_unref(L, LUA_REGISTRYINDEX, this->instanceRef_);
	luaL_unref(L, LUA_REGISTRYINDEX, this->environmentRef_);

	this->instanceRef_ = LUA_NOREF;
	this->environmentRef_ = LUA_NOREF;

	lua_gc(L, LUA_GCCOLLECT, 0);
}

void ReGuiEditor::LuaVM::update(float deltaTime) {
	this->callHook("onUpdate", deltaTime);
}

void ReGuiEditor::LuaVM::tick() {
	this->callHook("onTick");
}

ReGuiEditor::LuaVM* ReGuiEditor::LuaVM::getActive(lua_State* state) {
	lua_getfield(state, LUA_REGISTRYINDEX, ACTIVE_VM_KEY);
	LuaVM* vm = static_cast<LuaVM*>(lua_touserdata(state, -1));
	lua_pop(state, 1);

	return vm;
}

void ReGuiEditor::LuaVM::activate() {
	lua_pushlightuserdata(this->luaState_.getState(), this);
	lua_setfield(this->luaState_.getState(), LUA_REGISTRYINDEX, ACTIVE_VM_KEY);
}

ReGuiEditor::LuaResult ReGuiEditor::LuaVM::doFile(const std::string& path) {
	this->activate();
	return this->luaState_.doFile(path, this->modLocalId_, this->environmentRef_);
}

int ReGuiEditor::LuaVM::getGlobalField(const char* name) {
	lua_State* L = this->luaState_.getState();
	lua_rawgeti(L, LUA_REGISTRYINDEX, this->environmentRef_);
	lua_getfield(L, -1, name);
	lua_remove(L, -2);

	return lua_type(L, -1);
}

ReGuiEditor::LuaResult ReGuiEditor::LuaVM::call(int argCount, int resultCount) {
	this->activate();
	return this->luaState_.call(argCount, resultCount);
}

ReGuiEditor::LuaResult ReGuiEditor::LuaVM::createInstance(const std::string& className) {
	lua_State* L = this->luaState_.getState();

	const int type = this->getGlobalField(className.c_str());
	if (type == LUA_TNIL) {
		lua_pop(L, 1);
		return LuaResult::generateFailure("class '%s' is not defined by the script", className.c_str());
	}

	if (type != LUA_TTABLE) {
		const std::string typeName = luaL_typename(L, -1);
		lua_pop(L, 1);

		return LuaResult::generateFailure("'%s' must be a class, but it is a %s", className.c_str(), typeName.c_str());
	}

	LuaResult result = this->call(0, 1);
	if (!result)
		return result;

	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);

		return LuaResult::generateFailure("class '%s' did not create an instance", className.c_str());
	}

	luaL_unref(L, LUA_REGISTRYINDEX, this->instanceRef_);
	this->instanceRef_ = luaL_ref(L, LUA_REGISTRYINDEX);

	return LuaResult::generateSuccess();
}

bool ReGuiEditor::LuaVM::startInstance() {
	const LuaResult result = this->callMethod("onInit", 1);
	if (!result) {
		result.report();
		return false;
	}

	const std::optional<bool> shouldStart = this->luaState_.popValue<bool>();
	if (!shouldStart) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "onInit of mod %s must return a boolean.", this->modLocalId_.toString().c_str());
		return false;
	}

	return *shouldStart;
}

bool ReGuiEditor::LuaVM::pushMethod(const char* name) {
	if (!this->hasInstance())
		return false;

	lua_State* L = this->luaState_.getState();
	lua_rawgeti(L, LUA_REGISTRYINDEX, this->instanceRef_);
	lua_getfield(L, -1, name);
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		return false;
	}

	lua_insert(L, -2);
	return true;
}