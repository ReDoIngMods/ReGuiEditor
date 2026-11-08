#include "LuaClassInstance.hpp"

#include "LuaVM.hpp"

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <string>

bool LuaClassInstance::pushClass() {
	lua_State* L = this->vm_->getState();

	if (this->vm_->getField(this->className_) && lua_istable(L, -1))
		return true;

	lua_pop(L, 1);
	CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Class %s is not defined in the LuaVM.", this->className_.c_str());

	return false;
}

bool LuaClassInstance::create() {
	lua_State* L = this->vm_->getState();

	if (!this->pushClass())
		return false;

	if (!this->vm_->protectedCall(0, 1))
		return false;

	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Constructing class %s did not return a table.", this->className_.c_str());

		return false;
	}

	this->instanceRef_ = luaL_ref(L, LUA_REGISTRYINDEX);
	return this->callMethod("init", 0, 0);
}

bool LuaClassInstance::refresh() {
	lua_State* L = this->vm_->getState();

	if (!this->exists())
		return this->create();

	if (!this->pushClass())
		return false;

	lua_rawgeti(L, LUA_REGISTRYINDEX, this->instanceRef_);
	lua_pushvalue(L, -2);
	lua_setmetatable(L, -2);
	lua_pop(L, 2);

	return this->callMethod("refresh", 0, 0);
}

void LuaClassInstance::destroy() {
	if (!this->exists())
		return;

	luaL_unref(this->vm_->getState(), LUA_REGISTRYINDEX, this->instanceRef_);
	this->instanceRef_ = LUA_NOREF;
}

bool LuaClassInstance::callMethod(const std::string& methodName, int argCount, int resultCount) {
	lua_State* L = this->vm_->getState();

	if (!this->vm_->isInitialized() || !this->exists()) {
		lua_pop(L, argCount);

		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "No %s instance to call method %s on.", this->className_.c_str(), methodName.c_str());
		return false;
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, this->instanceRef_);
	lua_getfield(L, -1, methodName.c_str());

	if (!lua_isfunction(L, -1)) {
		lua_pop(L, argCount + 2);

		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Method %s does not exist on %s.", methodName.c_str(), this->className_.c_str());
		return false;
	}

	lua_insert(L, -(argCount + 2));
	lua_insert(L, -(argCount + 1));

	return this->vm_->protectedCall(argCount + 1, resultCount);
}