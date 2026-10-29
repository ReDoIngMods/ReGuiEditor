#pragma once
#include <JSON/JSONObject.hpp>

#include <LuaJIT/lua.h>

#include <string>

void wrap_sm_json(lua_State* L);

namespace SmJson {
	[[nodiscard]] bool toJson(lua_State* L, int index, JSONValue& output, std::string& error);
	void pushJson(lua_State* L, const JSONValue& value);

	int raiseError(lua_State* L, std::string& error);
}
