#include "wrap_sm_quat.hpp" // VCIC-Excluded

#include "../UserdataManager.hpp"

#include <UTILS/Quaternion.hpp>
#include <UTILS/Vector3.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <cstdio>

using UTILS::Quaternion;
using UTILS::Vector3;

constexpr float DEGREES_TO_RADIANS = 0.01745329251994329577f;

inline static Quaternion& checkQuat(lua_State* L, int index) {
	return UserdataManager::getInstance().check<Quaternion>(L, index);
}

inline static Vector3& checkVec3(lua_State* L, int index) {
	return UserdataManager::getInstance().check<Vector3>(L, index);
}

inline static float checkFloat(lua_State* L, int index) {
	return static_cast<float>(luaL_checknumber(L, index));
}

inline static int pushQuat(lua_State* L, const Quaternion& value) {
	UserdataManager::getInstance().create<Quaternion>(L, value);
	return 1;
}

inline static int pushVec3(lua_State* L, const Vector3& value) {
	UserdataManager::getInstance().create<Vector3>(L, value);
	return 1;
}

inline static int pushFloat(lua_State* L, float value) {
	lua_pushnumber(L, value);
	return 1;
}

// Constructors

static int quat_new(lua_State* L) { return pushQuat(L, { checkFloat(L, 1), checkFloat(L, 2), checkFloat(L, 3), checkFloat(L, 4) }); }
static int quat_identity(lua_State* L) { return pushQuat(L, Quaternion::identity()); }
static int quat_angleAxis(lua_State* L) { return pushQuat(L, Quaternion::angleAxis(checkFloat(L, 1), checkVec3(L, 2))); }
static int quat_fromEuler(lua_State* L) { return pushQuat(L, Quaternion::fromEuler(checkVec3(L, 1) * DEGREES_TO_RADIANS)); }
static int quat_lookRotation(lua_State* L) { return pushQuat(L, Quaternion::lookRotation(checkVec3(L, 1), checkVec3(L, 2))); }
static int quat_rotation(lua_State* L) { return pushQuat(L, Quaternion::fromTo(checkVec3(L, 1), checkVec3(L, 2))); }

// Components

static int quat_getX(lua_State* L) { return pushFloat(L, checkQuat(L, 1).x); }
static int quat_getY(lua_State* L) { return pushFloat(L, checkQuat(L, 1).y); }
static int quat_getZ(lua_State* L) { return pushFloat(L, checkQuat(L, 1).z); }
static int quat_getW(lua_State* L) { return pushFloat(L, checkQuat(L, 1).w); }

static int quat_setX(lua_State* L) { checkQuat(L, 1).x = checkFloat(L, 2); return 0; }
static int quat_setY(lua_State* L) { checkQuat(L, 1).y = checkFloat(L, 2); return 0; }
static int quat_setZ(lua_State* L) { checkQuat(L, 1).z = checkFloat(L, 2); return 0; }
static int quat_setW(lua_State* L) { checkQuat(L, 1).w = checkFloat(L, 2); return 0; }

// Math

static int quat_inverse(lua_State* L) { return pushQuat(L, checkQuat(L, 1).inverse()); }
static int quat_normalize(lua_State* L) { return pushQuat(L, checkQuat(L, 1).normalize()); }
static int quat_slerp(lua_State* L) { return pushQuat(L, Quaternion::slerp(checkQuat(L, 1), checkQuat(L, 2), checkFloat(L, 3))); }

// NOTE: sm.quat.getUp and sm.get.getAt are swapped in-game. If it doesnt match than switch them around!

static int quat_getRight(lua_State* L) { return pushVec3(L, checkQuat(L, 1).getRight()); }
static int quat_getUp(lua_State* L) { return pushVec3(L, checkQuat(L, 1).getForward()); }
static int quat_getAt(lua_State* L) { return pushVec3(L, checkQuat(L, 1).getUp()); }

// Metamethods

static int quat_mul(lua_State* L) {
	const Quaternion& left = checkQuat(L, 1);

	if (UserdataManager::getInstance().test<Vector3>(L, 2))
		return pushVec3(L, left * checkVec3(L, 2));

	return pushQuat(L, left * checkQuat(L, 2));
}

static int quat_eq(lua_State* L) {
	lua_pushboolean(L, checkQuat(L, 1) == checkQuat(L, 2));
	return 1;
}

static int quat_tostring(lua_State* L) {
	const Quaternion& quat = checkQuat(L, 1);

	char buffer[128];
	std::snprintf(buffer, sizeof(buffer), "Quat( %g, %g, %g, %g )", quat.x, quat.y, quat.z, quat.w);

	lua_pushstring(L, buffer);
	return 1;
}

void wrap_sm_quat(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.quat' library...");

	static const luaL_Reg functions[] = {
		{"getX", quat_getX},
		{"getY", quat_getY},
		{"getZ", quat_getZ},
		{"getW", quat_getW},

		{"setX", quat_setX},
		{"setY", quat_setY},
		{"setZ", quat_setZ},
		{"setW", quat_setW},

		{"inverse", quat_inverse},
		{"normalize", quat_normalize},
		{"slerp", quat_slerp},

		{"getRight", quat_getRight},
		{"getUp", quat_getUp},
		{"getAt", quat_getAt},

		{nullptr, nullptr}
	};

	static const luaL_Reg constructors[] = {
		{"new", quat_new},
		{"identity", quat_identity},
		{"angleAxis", quat_angleAxis},
		{"fromEuler", quat_fromEuler},
		{"lookRotation", quat_lookRotation},
		{"rotation", quat_rotation},

		{nullptr, nullptr}
	};

	UserdataDefinition* definition = UserdataManager::getInstance().createDefinition("Quat", 1001);
	definition->bind<Quaternion>();

	for (const luaL_Reg* function = functions; function->name; ++function)
		definition->addMethod(function->name, function->func);

	definition->addGetter("x", quat_getX);
	definition->addGetter("y", quat_getY);
	definition->addGetter("z", quat_getZ);
	definition->addGetter("w", quat_getW);

	definition->addSetter("x", quat_setX);
	definition->addSetter("y", quat_setY);
	definition->addSetter("z", quat_setZ);
	definition->addSetter("w", quat_setW);

	definition->addMetamethod("__mul", quat_mul);
	definition->addMetamethod("__eq", quat_eq);
	definition->addMetamethod("__tostring", quat_tostring);

	lua_newtable(L);
	luaL_register(L, nullptr, constructors);
	luaL_register(L, nullptr, functions);
	lua_setfield(L, -2, "quat");
}