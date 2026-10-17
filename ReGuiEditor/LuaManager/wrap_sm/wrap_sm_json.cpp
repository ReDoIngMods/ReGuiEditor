#include "wrap_sm_json.hpp" // VCIC-Excluded

#include "../LuaVM.hpp"

#include <FilesystemManager/FilesystemManager.hpp>

#include <UTILS/Strings.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <nlohmann/detail/output/serializer.hpp>
#include <nlohmann/detail/value_t.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>
#include <utility>
#include <vector>

inline static int raiseError(lua_State* L, std::string& error) {
	luaL_where(L, 1);
	lua_pushlstring(L, error.data(), error.size());
	lua_concat(L, 2);

	std::string().swap(error);
	return lua_error(L);
}

[[nodiscard]] inline static bool resolveModPath(lua_State* L, std::string& path, std::string& error) {
	const LuaVM* luaVM = LuaVM::fromState(L);
	if (!luaVM) {
		error = "LuaVM not found in registry.";
		return false;
	}

	const std::string& modLocalId = luaVM->getModLocalId();
	if (!FilesystemManager::getInstance().resolveContentData(path, modLocalId)) {
		error = UTILS::formatString("Failed to resolve path '%s.", path);
		return false;
	}

	return true;
}

inline static void pushJson(lua_State* L, const nlohmann::json& value, int depth) {
	switch (value.type()) {
	case nlohmann::json::value_t::object:
		lua_createtable(L, 0, static_cast<int>(value.size()));

		for (const auto& [key, child] : value.items()) {
			if (child.is_null())
				continue;

			lua_pushlstring(L, key.data(), key.size());
			pushJson(L, child, depth + 1);
			lua_rawset(L, -3);
		}

		break;
	case nlohmann::json::value_t::array: {
		lua_createtable(L, static_cast<int>(value.size()), 0);

		int index = 1;
		for (const auto& child : value) {
			pushJson(L, child, depth + 1);
			lua_rawseti(L, -2, index++);
		}

		break;
	}
	case nlohmann::json::value_t::string: {
		const auto& str = value.get_ref<const std::string&>();
		lua_pushlstring(L, str.data(), str.size());

		break;
	}
	case nlohmann::json::value_t::boolean:
		lua_pushboolean(L, value.get<bool>());
		break;
	case nlohmann::json::value_t::number_integer:
	case nlohmann::json::value_t::number_unsigned:
	case nlohmann::json::value_t::number_float:
		lua_pushnumber(L, value.get<lua_Number>());
		break;
	default:
		lua_pushnil(L);
		break;
	}
}

inline static bool parseJson(lua_State* L, const char* data, size_t length, std::string& error) {
	nlohmann::json document;

	try {
		document = nlohmann::json::parse(data, data + length);
	} catch (const nlohmann::json::parse_error& e) {
		// TODO: Make this accurate to the actual error message from SM

		error = e.what();
		return false;
	}

	pushJson(L, document, 0);
	return true;
}

inline static void toJson(lua_State* L, int index, int depth, nlohmann::json& output) {
	switch (lua_type(L, index)) {
	case LUA_TNIL:
		output = nullptr;
		return;
	case LUA_TBOOLEAN:
		output = lua_toboolean(L, index) != 0;
		return;
	case LUA_TNUMBER:
		output = lua_tonumber(L, index);
		return;
	case LUA_TSTRING:
		output = lua_tostring(L, index);
		return;
	case LUA_TTABLE:
		break;
	default:
		output = nullptr;
		return;
	}

	size_t entries = 0;
	lua_pushnil(L);
	while (lua_next(L, index)) {
		++entries;
		lua_pop(L, 1);
	}

	const int length = static_cast<int>(lua_objlen(L, index));
	bool isArray = entries == static_cast<size_t>(length);

	for (int objectIndex = 1; isArray && objectIndex <= length; ++objectIndex) {
		lua_rawgeti(L, index, objectIndex);
		isArray = !lua_isnil(L, -1);

		lua_pop(L, 1);
	}

	if (isArray) {
		output = nlohmann::json::array();

		for (int objectIndex = 1; objectIndex <= length; ++objectIndex) {
			lua_rawgeti(L, index, objectIndex);
			toJson(L, lua_gettop(L), depth + 1, output.emplace_back());
			lua_pop(L, 1);
		}

		return;
	}

	output = nlohmann::json::object();

	lua_pushnil(L);

	while (lua_next(L, index)) {
		if (lua_type(L, -2) != LUA_TSTRING) {
			lua_pop(L, 1);
			continue;
		}

		std::string key = lua_tostring(L, -2);
		toJson(L, lua_gettop(L), depth + 1, output[std::move(key)]);

		lua_pop(L, 1);
	}
}

inline static bool stringifyJson(lua_State* L, int index, bool pretty, std::string& output, std::string& error) {
	nlohmann::json document;
	toJson(L, index, 0, document);

	output = document.dump(pretty ? 4 : -1, ' ', false, nlohmann::json::error_handler_t::replace);
	return true;
}

inline static bool openJson(lua_State* L, const char* path, std::string& error) {
	std::string resolvedPath = path;
	if (!resolveModPath(L, resolvedPath, error))
		return false;

	std::vector<char> content;
	if (!FilesystemManager::getInstance().readFile(resolvedPath, content)) {
		error = "Failed to read file '" + resolvedPath + "'.";
		return false;
	}

	return parseJson(L, content.data(), content.size(), error);
}

[[nodiscard]] inline static bool saveJson(lua_State* L, int index, const char* path, std::string& error) {
	std::string resolvedPath = path;
	if (!resolveModPath(L, resolvedPath, error))
		return false;

	std::string output;
	if (!stringifyJson(L, index, false, output, error))
		return false;

	if (!FilesystemManager::getInstance().writeFile(resolvedPath, output)) {
		error = "Failed to write file '" + resolvedPath + "'.";
		return false;
	}

	return true;
}

static int sm_json_fileExists(lua_State* L) {
	std::string path = luaL_checkstring(L, 1);
	
	std::string error;
	if (!resolveModPath(L, path, error)) {
		lua_pushboolean(L, false);
		return 1;
	}

	std::vector<char> content;
	if (!FilesystemManager::getInstance().readFile(path, content)) {
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, true);
	return 1;
}

static int sm_json_open(lua_State* L) {
	const char* path = luaL_checkstring(L, 1);

	std::string error;
	if (!openJson(L, path, error))
		return raiseError(L, error);

	return 1;
}

static int sm_json_parseJsonString(lua_State* L) {
	size_t length = 0;
	const char* str = luaL_checklstring(L, 1, &length);

	std::string error;
	if (!parseJson(L, str, length, error))
		return raiseError(L, error);

	return 1;
}

static int sm_json_writeJsonString(lua_State* L) {
	luaL_checktype(L, 1, LUA_TTABLE);

	const bool pretty = lua_toboolean(L, 2) != 0;

	std::string output, error;
	if (!stringifyJson(L, 1, pretty, output, error))
		return raiseError(L, error);

	lua_pushlstring(L, output.data(), output.size());
	return 1;
}

static int sm_json_save(lua_State* L) {
	luaL_checktype(L, 1, LUA_TTABLE);

	const char* path = luaL_checkstring(L, 2);

	std::string error;
	if (!saveJson(L, 1, path, error))
		return raiseError(L, error);

	return 0;
}

void wrap_sm_json(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.json' library...");

	static const luaL_Reg functions[] = {
		{"fileExists", sm_json_fileExists},
		{"open", sm_json_open},
		{"parseJsonString", sm_json_parseJsonString},
		{"writeJsonString", sm_json_writeJsonString},
		{"save", sm_json_save},

		{nullptr, nullptr}
	};

	lua_newtable(L);
	luaL_register(L, nullptr, functions);
	lua_setfield(L, -2, "json");
}