#include "wrap_sm_json.hpp" // VCIC-Excluded

#include "../LuaUtil.hpp"

#include <Filesystem/FileInfo.hpp>
#include <Filesystem/FileReader.hpp>
#include <Filesystem/FileWriter.hpp>

#include <JSON/JSON.hpp>

#include <UTILS/Strings.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <filesystem>
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

inline static void pushJson(lua_State* L, const JSONValue& value) {
	switch (value.getType()) {
	case JSONType::None:
	case JSONType::Null:
		lua_pushnil(L);
		break;
	case JSONType::Boolean:
		lua_pushboolean(L, value.asBoolean());
		break;
	case JSONType::FloatingPoint:
		lua_pushnumber(L, value.asDouble());
		break;
	case JSONType::Integer:
		lua_pushinteger(L, value.asInteger());
		break;

	case JSONType::String:
		lua_pushstring(L, value.asString().c_str());
		break;
	case JSONType::Object: {
		const JSONObject& object = value.asObject();
		lua_createtable(L, 0, static_cast<int>(object.size()));

		for (const auto& [key, child] : object) {
			if (child.isNull())
				continue;

			lua_pushlstring(L, key.data(), key.size());
			pushJson(L, child);
			lua_rawset(L, -3);
		}

		break;
	}
	case JSONType::Array: {
		const JSONArray& array = value.asArray();

		lua_createtable(L, static_cast<int>(array.size()), 0);
		for (size_t index = 0; index < array.size(); ++index) {
			pushJson(L, array[index]);
			lua_rawseti(L, -2, static_cast<int>(index + 1));
		}

		break;
	}
	default:
		lua_pushnil(L);
		break;
	}
}

[[nodiscard]] static bool toJson(lua_State* L, int index, JSONValue& output, std::string& error) {
	if (index < 0 && index > LUA_REGISTRYINDEX)
		index = lua_gettop(L) + index + 1;

	switch (lua_type(L, index)) {
	case LUA_TBOOLEAN:
		output = lua_toboolean(L, index) != 0;
		return true;
	case LUA_TNUMBER:
		output = lua_tonumber(L, index);
		return true;
	case LUA_TSTRING:
		output = lua_tostring(L, index);
		return true;
	case LUA_TTABLE:
		break;
	default:
		output = nullptr;
		return true;
	}

	if (!lua_checkstack(L, 8)) {
		error = "Stack overflow while serializing table.";
		return false;
	}

	const size_t length = lua_objlen(L, index);

	size_t entries = 0;
	lua_pushnil(L);
	while (lua_next(L, index) != 0) {
		++entries;
		lua_pop(L, 1);
	}

	if (entries == length) {
		JSONArray array;
		array.reserve(length);

		for (size_t objectIndex = 1; objectIndex <= length; ++objectIndex) {
			lua_rawgeti(L, index, static_cast<int>(objectIndex));

			array.emplace_back();
			const bool success = toJson(L, -1, array.back(), error);

			lua_pop(L, 1);
			if (!success)
				return false;
		}

		output = std::move(array);
		return true;
	}

	JSONObject object;

	lua_pushnil(L);
	while (lua_next(L, index) != 0) {
		if (lua_type(L, -2) == LUA_TSTRING) {
			size_t keyLength = 0;
			const char* key = lua_tolstring(L, -2, &keyLength);

			JSONValue child;
			if (!toJson(L, -1, child, error)) {
				lua_pop(L, 2);
				return false;
			}

			object[std::string(key, keyLength)] = std::move(child);
		}

		lua_pop(L, 1);
	}

	output = std::move(object);
	return true;
}

[[nodiscard]] inline static bool stringifyJson(lua_State* L, int index, bool pretty, std::string& output, std::string& error) {
	JSONValue document;
	if (!toJson(L, index, document, error))
		return false;

	document.dump(output, pretty, '\t');
	return true;
}

[[nodiscard]] inline static bool openJson(lua_State* L, const char* path, std::string& error) {
	std::filesystem::path contentDataPath = path;
	LuaUtils::resolveContentDataPath(L, contentDataPath);

	auto document = Filesystem::readFileJson(contentDataPath);
	if (!document) {
		error = UTILS::formatString("Failed to read or parse JSON file '%s'.", contentDataPath.c_str());
		return false;
	}

	pushJson(L, document.value());
	return true;
}

[[nodiscard]] inline static bool saveJson(lua_State* L, int index, const char* path, std::string& error) {
	std::filesystem::path resolvedPath = path;
	LuaUtils::resolveContentDataPath(L, resolvedPath);

	std::string output;
	if (!stringifyJson(L, index, false, output, error))
		return false;

	if (!Filesystem::writeFile(resolvedPath, output)) {
		error = UTILS::formatString("Failed to write JSON file '%s'.", resolvedPath.c_str());
		return false;
	}

	return true;
}

static int sm_json_fileExists(lua_State* L) {
	std::filesystem::path path = luaL_checkstring(L, 1);

	std::string error;
	LuaUtils::resolveContentDataPath(L, path);

	lua_pushboolean(L, Filesystem::exists(path));
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
	std::string jsonString(str, length);

	JSONValue document;
	JSONParserResult result = JSONValue::parse(jsonString, document);
	if (!result) {
		std::string errorMessage = result.createErrorMessage();
		return raiseError(L, errorMessage);
	}

	pushJson(L, document);
	return 1;
}

static int sm_json_writeJsonString(lua_State* L) {
	luaL_checktype(L, 1, LUA_TTABLE);

	const bool pretty = lua_toboolean(L, 2) != 0;

	std::string output;
	std::string error;
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

bool SmJson::toJson(lua_State* L, int index, JSONValue& output, std::string& error) {
	return ::toJson(L, index, output, error);
}

void SmJson::pushJson(lua_State* L, const JSONValue& value) {
	::pushJson(L, value);
}

int SmJson::raiseError(lua_State* L, std::string& error) {
	return ::raiseError(L, error);
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
