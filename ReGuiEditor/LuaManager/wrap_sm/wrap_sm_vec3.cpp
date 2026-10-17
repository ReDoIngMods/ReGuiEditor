#include "wrap_sm_vec3.hpp" // VCIC-Excluded

#include "../UserdataManager.hpp"

#include <UTILS/Quaternion.hpp>
#include <UTILS/Vector3.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

inline static UTILS::Vector3& checkVec3(lua_State* L, int index) {
	return UserdataManager::getInstance().check<UTILS::Vector3>(L, index);
}

inline static float checkFloat(lua_State* L, int index) {
	return static_cast<float>(luaL_checknumber(L, index));
}

inline static int pushVec3(lua_State* L, const UTILS::Vector3& value) {
	UserdataManager::getInstance().create<UTILS::Vector3>(L, value);
	return 1;
}

inline static int pushFloat(lua_State* L, float value) {
	lua_pushnumber(L, value);
	return 1;
}

// Constructors

static int vec3_new(lua_State* L) { return pushVec3(L, { checkFloat(L, 1), checkFloat(L, 2), checkFloat(L, 3) }); }
static int vec3_zero(lua_State* L) { return pushVec3(L, { 0.0f, 0.0f, 0.0f }); }
static int vec3_one(lua_State* L) { return pushVec3(L, { 1.0f, 1.0f, 1.0f }); }

// Components

static int vec3_getX(lua_State* L) { return pushFloat(L, checkVec3(L, 1).x); }
static int vec3_getY(lua_State* L) { return pushFloat(L, checkVec3(L, 1).y); }
static int vec3_getZ(lua_State* L) { return pushFloat(L, checkVec3(L, 1).z); }

static int vec3_setX(lua_State* L) { checkVec3(L, 1).x = checkFloat(L, 2); return 0; }
static int vec3_setY(lua_State* L) { checkVec3(L, 1).y = checkFloat(L, 2); return 0; }
static int vec3_setZ(lua_State* L) { checkVec3(L, 1).z = checkFloat(L, 2); return 0; }

// Math

static int vec3_length(lua_State* L) { return pushFloat(L, checkVec3(L, 1).length()); }
static int vec3_length2(lua_State* L) { return pushFloat(L, checkVec3(L, 1).length2()); }
static int vec3_dot(lua_State* L) { return pushFloat(L, checkVec3(L, 1).dot(checkVec3(L, 2))); }
static int vec3_cross(lua_State* L) { return pushVec3(L, checkVec3(L, 1).cross(checkVec3(L, 2))); }
static int vec3_lerp(lua_State* L) { return pushVec3(L, checkVec3(L, 1).lerp(checkVec3(L, 2), checkFloat(L, 3))); }

static int vec3_normalize(lua_State* L) {
	const UTILS::Vector3& vector = checkVec3(L, 1);
	return pushVec3(L, vector / vector.length());
}

static int vec3_safeNormalize(lua_State* L) {
	const UTILS::Vector3& vector = checkVec3(L, 1);
	const float length = vector.length();

	return pushVec3(L, length > 0.0f ? vector / length : checkVec3(L, 2));
}

static int vec3_min(lua_State* L) {
	const UTILS::Vector3& a = checkVec3(L, 1);
	const UTILS::Vector3& b = checkVec3(L, 2);

	return pushVec3(L, { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) });
}

static int vec3_max(lua_State* L) {
	const UTILS::Vector3& a = checkVec3(L, 1);
	const UTILS::Vector3& b = checkVec3(L, 2);

	return pushVec3(L, { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) });
}

static int vec3_closestAxis(lua_State* L) {
	const UTILS::Vector3& vector = checkVec3(L, 1);
	const float ax = std::abs(vector.x), ay = std::abs(vector.y), az = std::abs(vector.z);

	if (ax >= ay && ax >= az)
		return pushVec3(L, { std::copysign(1.0f, vector.x), 0.0f, 0.0f });

	if (ay >= az)
		return pushVec3(L, { 0.0f, std::copysign(1.0f, vector.y), 0.0f });

	return pushVec3(L, { 0.0f, 0.0f, std::copysign(1.0f, vector.z) });
}

// Rotations (radians)

static int vec3_rotate(lua_State* L) { return pushVec3(L, checkVec3(L, 1).rotate(checkFloat(L, 2), checkVec3(L, 3))); }
static int vec3_rotateX(lua_State* L) { return pushVec3(L, checkVec3(L, 1).rotateX(checkFloat(L, 2))); }
static int vec3_rotateY(lua_State* L) { return pushVec3(L, checkVec3(L, 1).rotateY(checkFloat(L, 2))); }
static int vec3_rotateZ(lua_State* L) { return pushVec3(L, checkVec3(L, 1).rotateZ(checkFloat(L, 2))); }

// getRotation(from, to): shortest rotation between two directions
static int vec3_getRotation(lua_State* L) {
	UserdataManager::getInstance().create<UTILS::Quaternion>(L, UTILS::Quaternion::fromTo(checkVec3(L, 1), checkVec3(L, 2)));
	return 1;
}

// Metamethods

static int vec3_add(lua_State* L) { return pushVec3(L, checkVec3(L, 1) + checkVec3(L, 2)); }
static int vec3_sub(lua_State* L) { return pushVec3(L, checkVec3(L, 1) - checkVec3(L, 2)); }
static int vec3_unm(lua_State* L) { return pushVec3(L, -checkVec3(L, 1)); }

static int vec3_mul(lua_State* L) {
	if (lua_isnumber(L, 1))
		return pushVec3(L, checkFloat(L, 1) * checkVec3(L, 2));

	if (lua_isnumber(L, 2))
		return pushVec3(L, checkVec3(L, 1) * checkFloat(L, 2));

	return pushVec3(L, checkVec3(L, 1) * checkVec3(L, 2));
}

static int vec3_div(lua_State* L) {
	if (lua_isnumber(L, 2))
		return pushVec3(L, checkVec3(L, 1) / checkFloat(L, 2));

	return pushVec3(L, checkVec3(L, 1) / checkVec3(L, 2));
}

static int vec3_eq(lua_State* L) {
	lua_pushboolean(L, checkVec3(L, 1) == checkVec3(L, 2));
	return 1;
}

static int vec3_tostring(lua_State* L) {
	const UTILS::Vector3& vector = checkVec3(L, 1);

	char buffer[96];
	snprintf(buffer, sizeof(buffer), "Vec3( %g, %g, %g )", vector.x, vector.y, vector.z);

	lua_pushstring(L, buffer);
	return 1;
}

void wrap_sm_vec3(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.vec3' library...");

	static const luaL_Reg functions[] = {
		{"getX", vec3_getX},
		{"getY", vec3_getY},
		{"getZ", vec3_getZ},

		{"setX", vec3_setX},
		{"setY", vec3_setY},
		{"setZ", vec3_setZ},

		{"length", vec3_length},
		{"length2", vec3_length2},

		{"dot", vec3_dot},
		{"cross", vec3_cross},

		{"normalize", vec3_normalize},
		{"safeNormalize", vec3_safeNormalize},

		{"lerp", vec3_lerp},

		{"min", vec3_min},
		{"max", vec3_max},

		{"closestAxis", vec3_closestAxis}, {"getRotation", vec3_getRotation},

		{"rotate", vec3_rotate},
		{"rotateX", vec3_rotateX},
		{"rotateY", vec3_rotateY},
		{"rotateZ", vec3_rotateZ},

		{nullptr, nullptr}
	};

	static const luaL_Reg constructors[] = {
		{"new", vec3_new},
		{"zero", vec3_zero},
		{"one", vec3_one},

		{nullptr, nullptr}
	};

	UserdataDefinition* definition = UserdataManager::getInstance().createDefinition("Vec3", 1000);
	definition->bind<UTILS::Vector3>();

	for (const luaL_Reg* function = functions; function->name; ++function)
		definition->addMethod(function->name, function->func);

	definition->addGetter("x", vec3_getX);
	definition->addGetter("y", vec3_getY);
	definition->addGetter("z", vec3_getZ);

	definition->addSetter("x", vec3_setX);
	definition->addSetter("y", vec3_setY);
	definition->addSetter("z", vec3_setZ);

	definition->addMetamethod("__add", vec3_add);
	definition->addMetamethod("__sub", vec3_sub);
	definition->addMetamethod("__mul", vec3_mul);
	definition->addMetamethod("__div", vec3_div);
	definition->addMetamethod("__unm", vec3_unm);
	definition->addMetamethod("__eq", vec3_eq);
	definition->addMetamethod("__tostring", vec3_tostring);

	lua_newtable(L);
	luaL_register(L, nullptr, constructors);
	luaL_register(L, nullptr, functions);
	lua_setfield(L, -2, "vec3");
}