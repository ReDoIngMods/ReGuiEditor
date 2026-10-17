#include "wrap_sm.hpp" // VCIC-Excluded

#include "wrap_sm_color.hpp"
#include "wrap_sm_json.hpp"
#include "wrap_sm_log.hpp"
#include "wrap_sm_noise.hpp"
#include "wrap_sm_quat.hpp"
#include "wrap_sm_util.hpp"
#include "wrap_sm_uuid.hpp"
#include "wrap_sm_vec3.hpp"

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

static int sm_exists(lua_State* L) {
	if (lua_gettop(L) != 1)
		return luaL_error(L, "sm.exists expects exactly one argument");

	lua_pushboolean(L, !lua_isnil(L, 1));
	return 1;
}

static int sm_isServerMode(lua_State* L) {
	lua_pushboolean(L, false);
	return 1;
}

void wrap_sm(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm' library...");

	lua_newtable(L);

	lua_pushstring(L, "1.0.6.889");
	lua_setfield(L, -2, "version");

	static const luaL_Reg functions[] = {
		{"exists", sm_exists},
		{"isServerMode", sm_isServerMode},
		{nullptr, nullptr}
	};

	luaL_register(L, nullptr, functions);

	// sm.types
	struct TypeDef {
		const char* name;
		int value;
	};

	static constexpr TypeDef types[] = {
		{"nil", LUA_TNIL},
		{"boolean", LUA_TBOOLEAN},
		{"lightuserdata", LUA_TLIGHTUSERDATA},
		{"number", LUA_TNUMBER},
		{"string", LUA_TSTRING},
		{"table", LUA_TTABLE},
		{"function", LUA_TFUNCTION},
		{"userdata", LUA_TUSERDATA},
		{"thread", LUA_TTHREAD}
	};

	lua_newtable(L);

	for (const TypeDef& type : types) {
		lua_pushinteger(L, type.value);
		lua_setfield(L, -2, type.name);
	}

	lua_setfield(L, -2, "types");

	wrap_sm_color(L);
	wrap_sm_json(L);
	wrap_sm_log(L);
	wrap_sm_noise(L);
	wrap_sm_quat(L);
	wrap_sm_util(L);
	wrap_sm_uuid(L);
	wrap_sm_vec3(L);

	lua_setglobal(L, "sm");
}