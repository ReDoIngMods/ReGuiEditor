#include "LuaUtil.hpp" // VCIC-Excluded

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <string>
#include <utility>
#include <vector>

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
