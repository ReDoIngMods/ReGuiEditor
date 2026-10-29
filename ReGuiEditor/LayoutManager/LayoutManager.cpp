#include "LayoutManager.hpp"

#include "LayoutManagerView.hpp"

#include <GUI/ViewsManager.hpp>

#include <LuaManager/LuaManager.hpp>
#include <LuaManager/LuaVM.hpp>

#include <LuaJIT/lua.h>

#include <string>

void LayoutManager::initialize() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing LayoutManager...");

	ViewsManager& viewsManager = ViewsManager::getInstance();
	viewsManager.registerView<LayoutManagerView>("Layout Manager");
}

bool LayoutManager::loadLayout(const std::string& layoutName) {
	LuaVM* vm = LuaManager::getInstance().getReGuiVM();
	if (!vm->getField("LoadLayout")) {
		CONSOLE_ERROR("Failed to get 'LoadLayout' function from Lua VM.");
		return false;
	}

	lua_State* L = vm->getState();
	if (!lua_isfunction(L, -1)) {
		CONSOLE_ERROR("'LoadLayout' is not a function in Lua VM.");

		lua_pop(vm->getState(), 1);
		return false;
	}

	lua_pushstring(L, layoutName.c_str());

	if (!vm->protectedCall(1, 1)) {
		CONSOLE_ERROR("Failed to call 'LoadLayout' function in Lua VM.");
		return false;
	}

	if (!lua_isboolean(L, -1)) {
		CONSOLE_ERROR("'LoadLayout' did not return a boolean value.");

		lua_pop(L, 1);
		return false;
	}

	bool result = lua_toboolean(L, -1);
	lua_pop(L, 1);

	return result;
}
