#include "wrap_sm_vec3.hpp" // VCIC-Excluded

#include "../UserdataManager.hpp"

#include <UTILS/Quaternion.hpp>
#include <UTILS/Vector3.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <tuple>

static int vec3_new(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	if (userdataManager.test<UTILS::Vector3>(L, 1)) {
		userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1));
		return 1;
	}

	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const float z = static_cast<float>(luaL_checknumber(L, 3));

	userdataManager.create<UTILS::Vector3>(L, x, y, z);
	return 1;
}

static int vec3_zero(lua_State* L) {
	UserdataManager::getInstance().create<UTILS::Vector3>(L, UTILS::Vector3{ 0.0f, 0.0f, 0.0f });
	return 1;
}

static int vec3_one(lua_State* L) {
	UserdataManager::getInstance().create<UTILS::Vector3>(L, UTILS::Vector3{ 1.0f, 1.0f, 1.0f });
	return 1;
}

static int vec3_getX(lua_State* L) { lua_pushnumber(L, UserdataManager::getInstance().check<UTILS::Vector3>(L, 1).x); return 1; }
static int vec3_getY(lua_State* L) { lua_pushnumber(L, UserdataManager::getInstance().check<UTILS::Vector3>(L, 1).y); return 1; }
static int vec3_getZ(lua_State* L) { lua_pushnumber(L, UserdataManager::getInstance().check<UTILS::Vector3>(L, 1).z); return 1; }

static int vec3_setX(lua_State* L) { UserdataManager::getInstance().check<UTILS::Vector3>(L, 1).x = static_cast<float>(luaL_checknumber(L, 2)); return 0; }
static int vec3_setY(lua_State* L) { UserdataManager::getInstance().check<UTILS::Vector3>(L, 1).y = static_cast<float>(luaL_checknumber(L, 2)); return 0; }
static int vec3_setZ(lua_State* L) { UserdataManager::getInstance().check<UTILS::Vector3>(L, 1).z = static_cast<float>(luaL_checknumber(L, 2)); return 0; }

// Math

//static int vec3_length(lua_State* L) { return pushFloat(L, checkVec3(L, 1).length()); }
//static int vec3_length2(lua_State* L) { return pushFloat(L, checkVec3(L, 1).length2()); }
//static int vec3_dot(lua_State* L) { return pushFloat(L, checkVec3(L, 1).dot(checkVec3(L, 2))); }
//static int vec3_cross(lua_State* L) { return pushVec3(L, checkVec3(L, 1).cross(checkVec3(L, 2))); }
//static int vec3_lerp(lua_State* L) { return pushVec3(L, checkVec3(L, 1).lerp(checkVec3(L, 2), checkFloat(L, 3))); }

static int vec3_length(lua_State* L) {
	lua_pushnumber(L, UserdataManager::getInstance().check<UTILS::Vector3>(L, 1).length());
	return 1;
}

static int vec3_length2(lua_State* L) {
	lua_pushnumber(L, UserdataManager::getInstance().check<UTILS::Vector3>(L, 1).length2());
	return 1;
}

static int vec3_dot(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	lua_pushnumber(L, userdataManager.check<UTILS::Vector3>(L, 1).dot(userdataManager.check<UTILS::Vector3>(L, 2)));
	return 1;
}

static int vec3_cross(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).cross(userdataManager.check<UTILS::Vector3>(L, 2)));
	return 1;
}

static int vec3_lerp(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).lerp(userdataManager.check<UTILS::Vector3>(L, 2), static_cast<float>(luaL_checknumber(L, 3))));
	return 1;
}

static int vec3_abs(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).abs());
	return 1;
}

static int vec3_ceil(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).ceil());
	return 1;
}

static int vec3_floor(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).floor());
	return 1;
}

static int vec3_normalize(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).normalize());
	return 1;
}

static int vec3_safeNormalize(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).safeNormalize(userdataManager.check<UTILS::Vector3>(L, 2)));
	return 1;
}

static int vec3_min(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).min(userdataManager.check<UTILS::Vector3>(L, 2)));
	return 1;
}

static int vec3_max(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).max(userdataManager.check<UTILS::Vector3>(L, 2)));
	return 1;
}
static int vec3_closestAxis(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	const UTILS::Vector3& vector = userdataManager.check<UTILS::Vector3>(L, 1);
	const float ax = std::abs(vector.x), ay = std::abs(vector.y), az = std::abs(vector.z);
	if (ax >= ay && ax >= az) {
		userdataManager.create<UTILS::Vector3>(L, UTILS::Vector3{ std::copysign(1.0f, vector.x), 0.0f, 0.0f });
		return 1;
	}

	if (ay >= az) {
		userdataManager.create<UTILS::Vector3>(L, UTILS::Vector3{ 0.0f, std::copysign(1.0f, vector.y), 0.0f });
		return 1;
	}

	userdataManager.create<UTILS::Vector3>(L, UTILS::Vector3{ 0.0f, 0.0f, std::copysign(1.0f, vector.z) });
	return 1;
}

static int vec3_getDistance(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	lua_pushnumber(L, (userdataManager.check<UTILS::Vector3>(L, 1) - userdataManager.check<UTILS::Vector3>(L, 2)).length());
	return 1;
}

static int vec3_getDistanceSquared(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	lua_pushnumber(L, (userdataManager.check<UTILS::Vector3>(L, 1) - userdataManager.check<UTILS::Vector3>(L, 2)).length2());
	return 1;
}
static int vec3_fuzzyEquals(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();

	const UTILS::Vector3 delta = userdataManager.check<UTILS::Vector3>(L, 1) - userdataManager.check<UTILS::Vector3>(L, 2);
	lua_pushboolean(L, std::abs(delta.x) <= 1e-5f && std::abs(delta.y) <= 1e-5f && std::abs(delta.z) <= 1e-5f);

	return 1;
}

static int vec3_bezier2(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).bezier2(userdataManager.check<UTILS::Vector3>(L, 2), userdataManager.check<UTILS::Vector3>(L, 3), static_cast<float>(luaL_checknumber(L, 4))));
	return 1;
}

static int vec3_bezier3(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).bezier3(userdataManager.check<UTILS::Vector3>(L, 2), userdataManager.check<UTILS::Vector3>(L, 3), userdataManager.check<UTILS::Vector3>(L, 4), static_cast<float>(luaL_checknumber(L, 5))));
	return 1;
}

static int vec3_rotate(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).rotate(static_cast<float>(luaL_checknumber(L, 2)), userdataManager.check<UTILS::Vector3>(L, 3)));
	return 1;
}

static int vec3_rotateX(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).rotateX(static_cast<float>(luaL_checknumber(L, 2))));
	return 1;
}

static int vec3_rotateY(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).rotateY(static_cast<float>(luaL_checknumber(L, 2))));
	return 1;
}

static int vec3_rotateZ(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Vector3>(L, 1).rotateZ(static_cast<float>(luaL_checknumber(L, 2))));
	return 1;
}

static int vec3_getRotation(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Quaternion>(L, UTILS::Quaternion::fromTo(userdataManager.check<UTILS::Vector3>(L, 1), userdataManager.check<UTILS::Vector3>(L, 2)));
	return 1;
}

template <typename VectorOp, typename ScalarOp>
static int vec3_binary(lua_State* L, VectorOp vectorOp, ScalarOp scalarOp, bool commutative) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	if (lua_type(L, 2) == LUA_TNUMBER) {
		userdataManager.create<UTILS::Vector3>(L, scalarOp(userdataManager.check<UTILS::Vector3>(L, 1), static_cast<float>(luaL_checknumber(L, 2))));
		return 1;
	}

	if (commutative && lua_type(L, 1) == LUA_TNUMBER) {
		userdataManager.create<UTILS::Vector3>(L, scalarOp(userdataManager.check<UTILS::Vector3>(L, 2), static_cast<float>(luaL_checknumber(L, 1))));
		return 1;
	}

	userdataManager.create<UTILS::Vector3>(L, vectorOp(userdataManager.check<UTILS::Vector3>(L, 1), userdataManager.check<UTILS::Vector3>(L, 2)));
	return 1;
}

static int vec3_add(lua_State* L) {
	const auto vectorOp = [](const UTILS::Vector3& a, const UTILS::Vector3& b) -> UTILS::Vector3 { return a + b; };
	const auto scalarOp = [](const UTILS::Vector3& a, float s) -> UTILS::Vector3 { return { a.x + s, a.y + s, a.z + s }; };
	return vec3_binary(L, vectorOp, scalarOp, true);
}

static int vec3_sub(lua_State* L) {
	const auto vectorOp = [](const UTILS::Vector3& a, const UTILS::Vector3& b) -> UTILS::Vector3 { return a - b; };
	const auto scalarOp = [](const UTILS::Vector3& a, float s) -> UTILS::Vector3 { return { a.x - s, a.y - s, a.z - s }; };
	return vec3_binary(L, vectorOp, scalarOp, false);
}

static int vec3_mul(lua_State* L) {
	const auto vectorOp = [](const UTILS::Vector3& a, const UTILS::Vector3& b) -> UTILS::Vector3 { return a * b; };
	const auto scalarOp = [](const UTILS::Vector3& a, float s) -> UTILS::Vector3 { return a * s; };
	return vec3_binary(L, vectorOp, scalarOp, true);
}

static int vec3_div(lua_State* L) {
	const auto vectorOp = [](const UTILS::Vector3& a, const UTILS::Vector3& b) -> UTILS::Vector3 { return a / b; };
	const auto scalarOp = [](const UTILS::Vector3& a, float s) -> UTILS::Vector3 { return a / s; };
	return vec3_binary(L, vectorOp, scalarOp, false);
}

static int vec3_unm(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, -userdataManager.check<UTILS::Vector3>(L, 1));
	return 1;
}

static int vec3_eq(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	lua_pushboolean(L, userdataManager.check<UTILS::Vector3>(L, 1) == userdataManager.check<UTILS::Vector3>(L, 2));
	return 1;
}

static int vec3_lt(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	
	const UTILS::Vector3& a = userdataManager.check<UTILS::Vector3>(L, 1);
	const UTILS::Vector3& b = userdataManager.check<UTILS::Vector3>(L, 2);
	lua_pushboolean(L, std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z));

	return 1;
}

static int vec3_tostring(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	const UTILS::Vector3& vector = userdataManager.check<UTILS::Vector3>(L, 1);

	char buffer[96];
	snprintf(buffer, sizeof(buffer), "Vec3( %g, %g, %g )", vector.x, vector.y, vector.z);

	lua_pushstring(L, buffer);
	return 1;
}

void wrap_sm_vec3(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.vec3' library...");

	static const luaL_Reg methods[] = {
		{"abs", vec3_abs},
		{"ceil", vec3_ceil},
		{"floor", vec3_floor},
		{"length", vec3_length},
		{"length2", vec3_length2},
		{"dot", vec3_dot},
		{"cross", vec3_cross},
		{"min", vec3_min},
		{"max", vec3_max},
		{"normalize", vec3_normalize},
		{"safeNormalize", vec3_safeNormalize},
		{"rotate", vec3_rotate},
		{"rotateX", vec3_rotateX},
		{"rotateY", vec3_rotateY},
		{"rotateZ", vec3_rotateZ},

		{nullptr, nullptr}
	};

	static const luaL_Reg statics[] = {
		{"new", vec3_new},
		{"zero", vec3_zero},
		{"one", vec3_one},
		{"lerp", vec3_lerp},
		{"closestAxis", vec3_closestAxis},
		{"getRotation", vec3_getRotation},
		{"getDistance", vec3_getDistance},
		{"getDistanceSquared", vec3_getDistanceSquared},
		{"fuzzyEquals", vec3_fuzzyEquals},
		{"bezier2", vec3_bezier2},
		{"bezier3", vec3_bezier3},

		{nullptr, nullptr}
	};

	UserdataDefinition* definition = UserdataManager::getInstance().createDefinition("Vec3", 1000);
	definition->bind<UTILS::Vector3>();

	for (const luaL_Reg* method = methods; method->name; ++method)
		definition->addMethod(method->name, method->func);

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
	definition->addMetamethod("__lt", vec3_lt);
	definition->addMetamethod("__tostring", vec3_tostring);

	lua_newtable(L);
	luaL_register(L, nullptr, statics);
	lua_setfield(L, -2, "vec3");
}