#include "LayoutManager.hpp"

#include "LayoutManagerView.hpp"

#include <GUI/ViewsManager.hpp>

#include <LuaManager/LuaClassInstance.hpp>
#include <LuaManager/LuaManager.hpp>

#include <LuaJIT/lua.h>

#include <string>

void LayoutManager::initialize() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing LayoutManager...");

	ViewsManager& viewsManager = ViewsManager::getInstance();
	viewsManager.registerView<LayoutManagerView>("Layout Manager");
}

bool LayoutManager::loadLayout(const std::string& layoutName) {
	LuaManager& luaManager = LuaManager::getInstance();
	LuaClassInstance* loader = luaManager.getLoaderClassInstance();
	if (!loader || !loader->exists()) {
		CONSOLE_ERROR("No Loader class is loaded.");
		return false;
	}

	lua_State* vm = loader->getVM().getState();
	if (!vm) {
		CONSOLE_ERROR("No valid Lua state found.");
		return false;
	}

	lua_pushstring(vm, layoutName.c_str());
	if (!loader->callMethod("loadLayout", 1, 1)) {
		CONSOLE_ERROR("Failed to call 'loadLayout' method in Lua VM.");
		return false;
	}

	if (!lua_isboolean(vm, -1)) {
		CONSOLE_ERROR("'loadLayout' did not return a boolean value.");
	
		lua_pop(vm, 1);
		return false;
	}

	const bool result = lua_toboolean(vm, -1);
	lua_pop(vm, 1);

	return result;
}
