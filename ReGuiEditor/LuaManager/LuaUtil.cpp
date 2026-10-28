#include "LuaUtil.hpp" // VCIC-Excluded

#include "LuaVM.hpp"

#include <Filesystem/DirectoryManager.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

constexpr static const char* VM_REGISTRY_KEY = "VMPtr";
constexpr static uint32_t LUA_VM_STACK_LIMIT = 1024;

inline static const char* pushAsString(lua_State* L, int index, size_t& length) {
	if (lua_isuserdata(L, index) && luaL_getmetafield(L, index, "__tostring")) {
		if (lua_type(L, -1) == LUA_TSTRING)
			return lua_tolstring(L, -1, &length);

		lua_pop(L, 1);
	}

	lua_getglobal(L, "tostring");
	lua_pushvalue(L, index);
	lua_call(L, 1, 1);

	return lua_tolstring(L, -1, &length);
}

const char* LuaUtils::argsToString(lua_State* L, std::string& output) {
	output.clear();

	for (int index = 1, top = lua_gettop(L); index <= top; ++index) {
		if (index > 1)
			output += ' ';

		size_t length = 0;
		const char* str = pushAsString(L, index, length);
		if (!str) {
			lua_pop(L, 1);
			return "'tostring' must return a string";
		}

		output.append(str, length);
		lua_pop(L, 1);
	}

	return nullptr;
}

inline static void splitPath(const std::string& path, std::vector<std::string>& segments) {
	size_t start = 0;
	while (start < path.size()) {
		size_t end = path.find_first_of("/\\", start);
		if (end == std::string::npos)
			end = path.size();

		if (end > start)
			segments.push_back(path.substr(start, end - start));

		start = end + 1;
	}
}

bool LuaUtils::joinRelativePath(const std::string& baseFile, const std::string& relativePath, std::string& output) {
	std::vector<std::string> segments;
	splitPath(baseFile, segments);

	if (segments.size() < 2 || segments.front().front() != '$')
		return false;

	segments.pop_back();

	std::vector<std::string> relativeSegments;
	splitPath(relativePath, relativeSegments);

	for (std::string& segment : relativeSegments) {
		if (segment == ".")
			continue;

		if (segment == "..") {
			if (segments.size() == 1)
				return false;

			segments.pop_back();
			continue;
		}

		segments.push_back(std::move(segment));
	}

	output = segments.front();
	for (size_t index = 1; index < segments.size(); ++index)
		output += '/' + segments[index];

	return true;
}

bool LuaUtils::getCallerPath(lua_State* L, std::string& output) {
	lua_Debug debugInfo;
	for (int level = 1; lua_getstack(L, level, &debugInfo); ++level) {
		lua_getinfo(L, "S", &debugInfo);

		if (debugInfo.source && debugInfo.source[0] == '@') {
			output.assign(debugInfo.source + 1);
			return true;
		}
	}

	return false;
}

bool LuaUtils::resolveContentDataPath(lua_State* L, std::string& path) {
	if (path.empty())
		return false;

	if (!path.starts_with("$CONTENT_DATA"))
		return true;

	LuaVM* luaVM = LuaUtils::getLuaVM(L);
	if (!luaVM)
		return false;

	return DirectoryManager::getInstance().resolveContentData(path, luaVM->getModLocalId().c_str());
}

LuaVM* LuaUtils::getLuaVM(lua_State* L) {
	lua_getfield(L, LUA_REGISTRYINDEX, VM_REGISTRY_KEY);
	LuaVM* vm = static_cast<LuaVM*>(lua_touserdata(L, -1));
	lua_pop(L, 1);

	return vm;
}

void LuaUtils::setLuaVM(lua_State* L, LuaVM* vm) {
	lua_pushlightuserdata(L, vm);
	lua_setfield(L, LUA_REGISTRYINDEX, VM_REGISTRY_KEY);
}

void LuaUtils::dumpStack(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "--- LUA STACK DUMP ---");
	for (int index = 1, top = lua_gettop(L); index <= top; ++index) {
		switch (lua_type(L, index)) {
		case LUA_TSTRING:
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %s", index, lua_tostring(L, index));
			break;
		case LUA_TNUMBER:
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %g", index, lua_tonumber(L, index));
			break;
		case LUA_TBOOLEAN:
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %s", index, lua_toboolean(L, index) ? "true" : "false");
			break;
		default:
			CONSOLE_LOG(LEVEL_LUA, COLOR_YELLOW, "[%d]: %s", index, luaL_typename(L, index));
			break;
		}
	}
}