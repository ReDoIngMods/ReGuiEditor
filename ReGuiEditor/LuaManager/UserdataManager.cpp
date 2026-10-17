#include "UserdataManager.hpp"

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

template <typename Entries>
static void pushTable(lua_State* L, const Entries& entries) {
	lua_createtable(L, 0, static_cast<int>(entries.size()));

	for (const auto& entry : entries) {
		lua_pushcfunction(L, entry.function);
		lua_setfield(L, -2, entry.name.c_str());
	}
}

int UserdataManager::indexDispatch(lua_State* L) {
	if (lua_type(L, 2) == LUA_TSTRING) {
		lua_pushvalue(L, 2);
		lua_rawget(L, lua_upvalueindex(1));

		if (!lua_isnil(L, -1)) {
			lua_pushvalue(L, 1);
			lua_call(L, 1, 1);

			return 1;
		}

		lua_pushvalue(L, 2);
		lua_rawget(L, lua_upvalueindex(2));
		if (!lua_isnil(L, -1))
			return 1;
	}

	lua_pushnil(L);
	return 1;
}

int UserdataManager::newIndexDispatch(lua_State* L) {
	if (lua_type(L, 2) == LUA_TSTRING) {
		lua_pushvalue(L, 2);
		lua_rawget(L, lua_upvalueindex(1));

		if (!lua_isnil(L, -1)) {
			lua_pushvalue(L, 1);
			lua_pushvalue(L, 3);
			lua_call(L, 2, 0);

			return 0;
		}
	}

	return luaL_error(L, "cannot set '%s' on %s", lua_tostring(L, 2), lua_tostring(L, lua_upvalueindex(2)));
}

UserdataDefinition* UserdataManager::createDefinition(const std::string& name, int typeId) {
	if (this->getDefinition(name)) {
		CONSOLE_LOG(LEVEL_LUA, COLOR_RED, "Userdata definition '%s' already exists.", name.c_str());
		return nullptr;
	}

	this->definitions_.push_back(std::make_unique<UserdataDefinition>(name, typeId));
	return this->definitions_.back().get();
}

UserdataDefinition* UserdataManager::getDefinition(const std::string& name) const {
	auto iterator = std::find_if(this->definitions_.begin(), this->definitions_.end(), [&name](const std::unique_ptr<UserdataDefinition>& definition) {
		return definition->getName() == name;
	});

	return (iterator != this->definitions_.end()) ? iterator->get() : nullptr;
}

void UserdataManager::registerDefinitions(lua_State* L) {
	for (const auto& definition : this->definitions_) {
		if (!definition->registered_)
			UserdataManager::registerMetatable(L, *definition);
	}
}

void UserdataManager::registerMetatable(lua_State* L, UserdataDefinition& definition) {
	luaL_newmetatable(L, definition.name_.c_str());

	for (const auto& metamethod : definition.metamethods_) {
		lua_pushcfunction(L, metamethod.function);
		lua_setfield(L, -2, metamethod.name.c_str());
	}

	lua_pushstring(L, definition.name_.c_str());
	lua_setfield(L, -2, "__name");

	lua_pushinteger(L, definition.typeId_);
	lua_setfield(L, -2, "__typeid");

	pushTable(L, definition.getters_);
	pushTable(L, definition.methods_);
	lua_pushcclosure(L, UserdataManager::indexDispatch, 2);
	lua_setfield(L, -2, "__index");

	pushTable(L, definition.setters_);
	lua_pushstring(L, definition.name_.c_str());
	lua_pushcclosure(L, UserdataManager::newIndexDispatch, 2);
	lua_setfield(L, -2, "__newindex");

	if (definition.destructor_) {
		lua_pushcfunction(L, definition.destructor_);
		lua_setfield(L, -2, "__gc");
	}

	lua_pop(L, 1);

	lua_getglobal(L, "sm");
	lua_getfield(L, -1, "types");
	lua_pushinteger(L, definition.typeId_);
	lua_pushstring(L, definition.name_.c_str());
	lua_settable(L, -3);
	lua_pop(L, 2);

	definition.registered_ = true;
}