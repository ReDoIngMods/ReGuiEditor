#include "wrap_sm_uuid.hpp" // VCIC-Excluded

#include "../UserdataManager.hpp"

#include <Modules/UniqueIdentifiers/Uuid.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <string>
#include <string_view>

using Uuid = ReGuiEditor::Uuid;

static int uuid_new(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	if (lua_gettop(L) == 0) {
		userdataManager.create<Uuid>(L, Uuid::generateRandom());
		return 1;
	}

	if (userdataManager.test<Uuid>(L, 1)) {
		userdataManager.create<Uuid>(L, userdataManager.check<Uuid>(L, 1));
		return 1;
	}

	size_t length = 0;
	const char* text = luaL_checklstring(L, 1, &length);
	const std::string_view uuidString(text, length);

	Uuid uuid;
	if (!Uuid::fromString(uuidString, uuid))
		return luaL_error(L, "invalid uuid '%s'", text);

	userdataManager.create<Uuid>(L, uuid);
	return 1;
}

static int uuid_getNil(lua_State* L) {
	UserdataManager::getInstance().create<Uuid>(L, Uuid());
	return 1;
}

static int uuid_generateRandom(lua_State* L) {
	UserdataManager::getInstance().create<Uuid>(L, Uuid::generateRandom());
	return 1;
}

static int uuid_generateNamed(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();

	size_t length = 0;
	const char* name = luaL_checklstring(L, 2, &length);
	const std::string_view nameView(name, length);

	Uuid uuidNamespace = userdataManager.check<Uuid>(L, 1);
	Uuid uuid = Uuid::generateNamed(uuidNamespace, nameView);
	userdataManager.create<Uuid>(L, uuid);

	return 1;
}

static int uuid_isNil(lua_State* L) {
	lua_pushboolean(L, UserdataManager::getInstance().check<Uuid>(L, 1).isNil());
	return 1;
}

// Metamethods

static int uuid_eq(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	lua_pushboolean(L, userdataManager.check<Uuid>(L, 1) == userdataManager.check<Uuid>(L, 2));

	return 1;
}

static int uuid_tostring(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();

	const std::string text = userdataManager.check<Uuid>(L, 1).toString();
	lua_pushlstring(L, text.data(), text.size());

	return 1;
}

void wrap_sm_uuid(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.uuid' library...");

	static const luaL_Reg constructors[] = {
		{"new", uuid_new},
		{"getNil", uuid_getNil},
		{"generateRandom", uuid_generateRandom},
		{"generateNamed", uuid_generateNamed},

		{nullptr, nullptr}
	};

	UserdataDefinition* definition = UserdataManager::getInstance().createDefinition("Uuid", 1003);
	definition->bind<Uuid>();

	definition->addMethod("isNil", uuid_isNil);

	definition->addMetamethod("__eq", uuid_eq);
	definition->addMetamethod("__tostring", uuid_tostring);

	lua_newtable(L);
	luaL_register(L, nullptr, constructors);
	lua_setfield(L, -2, "uuid");
}
