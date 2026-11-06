#include "wrap_sm_quat.hpp" // VCIC-Excluded

#include "../UserdataManager.hpp"

#include <UTILS/Quaternion.hpp>
#include <UTILS/Vector3.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

constexpr float DEGREES_TO_RADIANS = 0.01745329251994329577f;
constexpr float HALF_PI = 1.57079632679489661923f;

static int quat_new(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	if (userdataManager.test<UTILS::Quaternion>(L, 1)) {
		UTILS::Quaternion& quat = userdataManager.check<UTILS::Quaternion>(L, 1);
		userdataManager.create<UTILS::Quaternion>(L, quat);

		return 1;
	}

	const float x = static_cast<float>(luaL_checknumber(L, 1));
	const float y = static_cast<float>(luaL_checknumber(L, 2));
	const float z = static_cast<float>(luaL_checknumber(L, 3));
	const float w = static_cast<float>(luaL_checknumber(L, 4));

	userdataManager.create<UTILS::Quaternion>(L, x, y, z, w);
	return 1;
}

static int quat_identity(lua_State* L) {
	UserdataManager::getInstance().create<UTILS::Quaternion>(L, UTILS::Quaternion::identity());
	return 1;
}

static int quat_angleAxis(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Quaternion>(L, UTILS::Quaternion::angleAxis(static_cast<float>(luaL_checknumber(L, 1)), userdataManager.check<UTILS::Vector3>(L, 2)));

	return 1;
}

static int quat_fromEuler(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Quaternion>(L, UTILS::Quaternion::fromEuler(userdataManager.check<UTILS::Vector3>(L, 1) * DEGREES_TO_RADIANS));
	
	return 1;
}

static int quat_lookRotation(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();

	UTILS::Vector3& forward = userdataManager.check<UTILS::Vector3>(L, 1);
	UTILS::Vector3& up = userdataManager.check<UTILS::Vector3>(L, 2);
	userdataManager.create<UTILS::Quaternion>(L, UTILS::Quaternion::lookRotation(forward, up));

	return 1;
}

static int quat_slerp(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();

	UTILS::Quaternion& from = userdataManager.check<UTILS::Quaternion>(L, 1);
	UTILS::Quaternion& to = userdataManager.check<UTILS::Quaternion>(L, 2);
	const float t = static_cast<float>(luaL_checknumber(L, 3));
	userdataManager.create<UTILS::Quaternion>(L, UTILS::Quaternion::slerp(from, to, t));

	return 1;
}

inline static UTILS::Quaternion axisRotation(int axis, int quarterTurns) {
	static const UTILS::Vector3 AXES[3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
	return UTILS::Quaternion::angleAxis(static_cast<float>(quarterTurns) * HALF_PI, AXES[axis]);
}

template <int Axis, int QuarterTurns>
static int quat_fixedRotation(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Quaternion>(L, axisRotation(Axis, QuarterTurns));

	return 1;
}

static int quat_getX(lua_State* L) { lua_pushnumber(L, UserdataManager::getInstance().check<UTILS::Quaternion>(L, 1).x); return 1; }
static int quat_getY(lua_State* L) { lua_pushnumber(L, UserdataManager::getInstance().check<UTILS::Quaternion>(L, 1).y); return 1; }
static int quat_getZ(lua_State* L) { lua_pushnumber(L, UserdataManager::getInstance().check<UTILS::Quaternion>(L, 1).z); return 1; }
static int quat_getW(lua_State* L) { lua_pushnumber(L, UserdataManager::getInstance().check<UTILS::Quaternion>(L, 1).w); return 1; }

static int quat_setX(lua_State* L) { UserdataManager::getInstance().check<UTILS::Quaternion>(L, 1).x = static_cast<float>(luaL_checknumber(L, 2)); return 0; }
static int quat_setY(lua_State* L) { UserdataManager::getInstance().check<UTILS::Quaternion>(L, 1).y = static_cast<float>(luaL_checknumber(L, 2)); return 0; }
static int quat_setZ(lua_State* L) { UserdataManager::getInstance().check<UTILS::Quaternion>(L, 1).z = static_cast<float>(luaL_checknumber(L, 2)); return 0; }
static int quat_setW(lua_State* L) { UserdataManager::getInstance().check<UTILS::Quaternion>(L, 1).w = static_cast<float>(luaL_checknumber(L, 2)); return 0; }

static int quat_inverse(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Quaternion>(L, userdataManager.check<UTILS::Quaternion>(L, 1).inverse());

	return 1;
}

static int quat_normalize(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Quaternion>(L, userdataManager.check<UTILS::Quaternion>(L, 1).normalize());

	return 1;
}

static int quat_safeNormalize(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();

	UTILS::Quaternion& quat = userdataManager.check<UTILS::Quaternion>(L, 1);
	UTILS::Quaternion& fallback = userdataManager.check<UTILS::Quaternion>(L, 2);

	if (quat.length2() > 1e-6f)
		userdataManager.create<UTILS::Quaternion>(L, quat.normalize());
	else
		userdataManager.create<UTILS::Quaternion>(L, fallback);

	return 1;
}

// Snaps to the closest of the 24 axis-aligned orientations.
static int quat_round90(lua_State* L) {
	static const std::vector<UTILS::Quaternion> ORIENTATIONS = [] {
		std::vector<UTILS::Quaternion> orientations;

		for (int x = 0; x < 4; ++x)
			for (int y = 0; y < 4; ++y)
				for (int z = 0; z < 4; ++z) {
					const UTILS::Quaternion candidate = axisRotation(0, x) * axisRotation(1, y) * axisRotation(2, z);

					const bool duplicate = std::any_of(orientations.begin(), orientations.end(), [&](const UTILS::Quaternion& other) -> bool {
						return std::abs(other.x * candidate.x + other.y * candidate.y + other.z * candidate.z + other.w * candidate.w) > 0.999f;
					});

					if (!duplicate)
						orientations.push_back(candidate);
				}

		return orientations;
	}();

	UserdataManager& userdataManager = UserdataManager::getInstance();
	const UTILS::Quaternion& quat = userdataManager.check<UTILS::Quaternion>(L, 1);

	const UTILS::Quaternion* best = &ORIENTATIONS.front();
	float bestDot = -1.0f;

	for (const UTILS::Quaternion& candidate : ORIENTATIONS) {
		const float dot = std::abs(quat.dot(candidate));
		if (dot > bestDot) {
			bestDot = dot;
			best = &candidate;
		}
	}

	userdataManager.create<UTILS::Quaternion>(L, *best);
	return 1;
}

// NOTE: In-game getUp is the local Y axis and getAt is the local Z axis. If this doesn't match the Quaternion class's convention, swap them here.

static int quat_getRight(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Quaternion>(L, 1).getRight());

	return 1;
}

static int quat_getUp(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Quaternion>(L, 1).getForward());

	return 1;
}

static int quat_getAt(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	userdataManager.create<UTILS::Vector3>(L, userdataManager.check<UTILS::Quaternion>(L, 1).getUp());
	return 1;
}

// Metamethods

static int quat_mul(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();

	const UTILS::Quaternion& left = userdataManager.check<UTILS::Quaternion>(L, 1);
	if (userdataManager.test<UTILS::Vector3>(L, 2)) {
		userdataManager.create<UTILS::Vector3>(L, left * userdataManager.check<UTILS::Vector3>(L, 2));
		return 1;
	}

	userdataManager.create<UTILS::Quaternion>(L, left * userdataManager.check<UTILS::Quaternion>(L, 2));
	return 1;
}

static int quat_eq(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	lua_pushboolean(L, userdataManager.check<UTILS::Quaternion>(L, 1) == userdataManager.check<UTILS::Quaternion>(L, 2));

	return 1;
}

static int quat_tostring(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();
	const UTILS::Quaternion& quat = userdataManager.check<UTILS::Quaternion>(L, 1);
	
	char buffer[128];
	std::snprintf(buffer, sizeof(buffer), "Quat( %g, %g, %g, %g )", quat.x, quat.y, quat.z, quat.w);
	
	lua_pushstring(L, buffer);
	return 1;
}

void wrap_sm_quat(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.quat' library...");

	static const luaL_Reg methods[] = {
		{"getRight", quat_getRight},
		{"getUp", quat_getUp},
		{"getAt", quat_getAt},
		{"inverse", quat_inverse},
		{"normalize", quat_normalize},
		{"safeNormalize", quat_safeNormalize},
		{"round90", quat_round90},

		{nullptr, nullptr}
	};

	static const luaL_Reg statics[] = {
		{"new", quat_new},
		{"identity", quat_identity},
		{"angleAxis", quat_angleAxis},
		{"fromEuler", quat_fromEuler},
		{"lookRotation", quat_lookRotation},
		{"slerp", quat_slerp},

		{"rotX90", quat_fixedRotation<0, 1>},
		{"rotNegX90", quat_fixedRotation<0, -1>},
		{"rotX180", quat_fixedRotation<0, 2>},
		{"rotY90", quat_fixedRotation<1, 1>},
		{"rotNegY90", quat_fixedRotation<1, -1>},
		{"rotY180", quat_fixedRotation<1, 2>},
		{"rotZ90", quat_fixedRotation<2, 1>},
		{"rotNegZ90", quat_fixedRotation<2, -1>},
		{"rotZ180", quat_fixedRotation<2, 2>},

		{nullptr, nullptr}
	};

	UserdataDefinition* definition = UserdataManager::getInstance().createDefinition("Quat", 1001);
	definition->bind<UTILS::Quaternion>();

	for (const luaL_Reg* method = methods; method->name; ++method)
		definition->addMethod(method->name, method->func);

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
	luaL_register(L, nullptr, statics);
	lua_setfield(L, -2, "quat");
}
