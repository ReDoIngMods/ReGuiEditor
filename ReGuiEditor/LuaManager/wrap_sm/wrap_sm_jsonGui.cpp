#include "wrap_sm_jsonGui.hpp" // VCIC-Excluded

#include "wrap_sm_json.hpp"

#include "../LuaManager.hpp"
#include "../LuaUtil.hpp"
#include "../LuaVM.hpp"
#include "../UserdataManager.hpp"

#include <JSON/JSONObject.hpp>

#include <MyGuiRenderer/GuiSystemManager.hpp>
#include <MyGuiRenderer/JsonGui/JsonGuiTypes.hpp>
#include <MyGuiRenderer/JsonGuiInstance.hpp>

#include <MyGui/MyGUI_Types.h>
#include <MyGui/MyGUI_Widget.h>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

static constexpr double VIEW_HEIGHT = 720.0;

struct JsonGuiHandle {
	std::shared_ptr<JsonGuiInstance> gui;
};

struct JsonWidgetHandle {
	std::shared_ptr<JsonGuiInstance> gui;
	std::string name;
};

inline static void readBool(lua_State* L, int table, const char* key, bool& value) {
	lua_getfield(L, table, key);
	if (!lua_isnil(L, -1))
		value = lua_toboolean(L, -1) != 0;

	lua_pop(L, 1);
}

inline static void readNumber(lua_State* L, int table, const char* key, float& value) {
	lua_getfield(L, table, key);
	if (lua_isnumber(L, -1))
		value = static_cast<float>(lua_tonumber(L, -1));

	lua_pop(L, 1);
}

inline static void readString(lua_State* L, int table, const char* key, std::string& value) {
	lua_getfield(L, table, key);
	if (lua_type(L, -1) == LUA_TSTRING) {
		size_t length = 0;
		const char* string = lua_tolstring(L, -1, &length);

		value.assign(string, length);
	}

	lua_pop(L, 1);
}

inline static JsonGuiInstance& checkGui(lua_State* L, int index) {
	return *UserdataManager::getInstance().check<JsonGuiHandle>(L, index).gui;
}

static void dispatchGuiEvent(const std::string& modLocalId, const GuiEvent& event) {
	LuaVM* vm = LuaManager::getInstance().getVM(modLocalId);
	if (!vm || !vm->isInitialized())
		return;

	lua_State* L = vm->getState();
	if (!lua_checkstack(L, 4))
		return;

	lua_pushlstring(L, event.widget.data(), event.widget.size());
	int argCount = 1;

	switch (event.type) {
	case GuiEventType::TextEdit:
	case GuiEventType::TextEnter:
		lua_pushlstring(L, event.text.data(), event.text.size());

		argCount = 2;
		break;
	case GuiEventType::Drag:
		lua_pushinteger(L, event.x);
		lua_pushinteger(L, event.y);

		argCount = 3;
		break;
	case GuiEventType::MouseWheel:
		lua_pushinteger(L, event.x);

		argCount = 2;
		break;
	default:
		break;
	}

	(void)vm->callFunction(event.handler, argCount, 0);
}

static int jsonGui_createGui(lua_State* L) {
	GuiSettings settings;
	if (!lua_isnoneornil(L, 1)) {
		luaL_checktype(L, 1, LUA_TTABLE);

		readBool(L, 1, "isHud", settings.isHud);
		readBool(L, 1, "isInteractive", settings.isInteractive);
		readBool(L, 1, "needsCursor", settings.needsCursor);
		readBool(L, 1, "hidesHotbar", settings.hidesHotbar);
		readBool(L, 1, "isOverlapped", settings.isOverlapped);
		readBool(L, 1, "openCloseAudio", settings.openCloseAudio);

		readNumber(L, 1, "backgroundAlpha", settings.backgroundAlpha);

		readString(L, 1, "handleKeySetup", settings.handleKeySetup);
		readString(L, 1, "name", settings.name);
		readString(L, 1, "layer", settings.layer);
	}

	const LuaVM* creator = LuaUtils::getLuaVM(L);
	if (!creator)
		creator = LuaManager::getInstance().getReGuiVM();

	auto gui = std::make_shared<JsonGuiInstance>(std::move(settings));
	if (creator) {
		gui->setEventSink([modLocalId = creator->getModLocalId()](const GuiEvent& event) {
			dispatchGuiEvent(modLocalId, event);
		});
	}

	JsonGuiHandle handle;
	handle.gui = std::move(gui);

	UserdataManager::getInstance().create<JsonGuiHandle>(L, std::move(handle));
	return 1;
}

static int jsonGui_getViewSize(lua_State* L) {
	const GuiSystemManager& guiSystem = GuiSystemManager::getInstance();

	const uint32_t width = guiSystem.getRenderTargetWidth();
	const uint32_t height = guiSystem.getRenderTargetHeight();
	if (width == 0 || height == 0) {
		lua_pushinteger(L, 0);
		lua_pushinteger(L, 0);

		return 2;
	}

	lua_pushinteger(L, static_cast<lua_Integer>(std::llround(VIEW_HEIGHT * width / height)));
	lua_pushinteger(L, static_cast<lua_Integer>(VIEW_HEIGHT));
	return 2;
}

static int jsonGui_render(lua_State* L) {
	JsonGuiInstance& gui = checkGui(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);

	JSONValue document;
	std::string error;
	if (!SmJson::toJson(L, 2, document, error))
		return SmJson::raiseError(L, error);

	gui.render(document);
	return 0;
}

static int jsonGui_isActive(lua_State* L) {
	lua_pushboolean(L, checkGui(L, 1).isActive());
	return 1;
}

static int jsonGui_close(lua_State* L) {
	checkGui(L, 1).close();
	return 0;
}

static int jsonGui_setHidden(lua_State* L) {
	JsonGuiInstance& gui = checkGui(L, 1);
	luaL_checktype(L, 2, LUA_TBOOLEAN);

	gui.setHidden(lua_toboolean(L, 2) != 0);
	return 0;
}

static int jsonGui_isHidden(lua_State* L) {
	lua_pushboolean(L, checkGui(L, 1).isHidden());
	return 1;
}

static int jsonGui_clearOnCloseCallback(lua_State* L) {
	checkGui(L, 1).clearOnCloseCallback();
	return 0;
}

static int jsonGui_getWidgetAbsolutePosition(lua_State* L) {
	const JsonGuiInstance& gui = checkGui(L, 1);

	const MyGUI::Widget* widget = gui.findWidget(luaL_checkstring(L, 2));
	if (!widget) {
		lua_pushnil(L);
		return 1;
	}

	const MyGUI::IntPoint& position = widget->getAbsolutePosition();
	lua_pushinteger(L, position.left);
	lua_pushinteger(L, position.top);

	return 2;
}

static int jsonGui_getWidget(lua_State* L) {
	UserdataManager& userdataManager = UserdataManager::getInstance();

	const JsonGuiHandle& handle = userdataManager.check<JsonGuiHandle>(L, 1);
	const char* name = luaL_checkstring(L, 2);

	if (!handle.gui->findWidget(name)) {
		lua_pushnil(L);
		return 1;
	}

	userdataManager.create<JsonWidgetHandle>(L, JsonWidgetHandle{ handle.gui, name });
	return 1;
}

static int jsonWidget_startEffect(lua_State* L) {
	const JsonWidgetHandle& handle = UserdataManager::getInstance().check<JsonWidgetHandle>(L, 1);
	const char* effect = luaL_checkstring(L, 2);

	MyGUI::Widget* widget = handle.gui->findWidget(handle.name);
	if (!widget)
		return luaL_error(L, "widget '%s' no longer exists", handle.name.c_str());

	widget->setProperty("EffectName", effect);
	widget->setProperty("PlayState", "Play");
	return 0;
}

void wrap_sm_jsonGui(lua_State* L) {
	CONSOLE_LOG(LEVEL_LUA, COLOR_CYAN, "Wrapping 'sm.jsonGui' library...");

	static const luaL_Reg functions[] = {
		{"createGui", jsonGui_createGui},
		{"getViewSize", jsonGui_getViewSize},

		{nullptr, nullptr}
	};

	UserdataManager& userdataManager = UserdataManager::getInstance();

	UserdataDefinition* guiDefinition = userdataManager.createDefinition("JsonGui", 1018);
	guiDefinition->bind<JsonGuiHandle>();

	guiDefinition->addMethod("render", jsonGui_render);
	guiDefinition->addMethod("isActive", jsonGui_isActive);
	guiDefinition->addMethod("close", jsonGui_close);
	guiDefinition->addMethod("setHidden", jsonGui_setHidden);
	guiDefinition->addMethod("isHidden", jsonGui_isHidden);
	guiDefinition->addMethod("clearOnCloseCallback", jsonGui_clearOnCloseCallback);
	guiDefinition->addMethod("getWidgetAbsolutePosition", jsonGui_getWidgetAbsolutePosition);
	guiDefinition->addMethod("getWidget", jsonGui_getWidget);

	UserdataDefinition* widgetDefinition = userdataManager.createDefinition("JsonWidget", 1019);
	widgetDefinition->bind<JsonWidgetHandle>();

	widgetDefinition->addMethod("startEffect", jsonWidget_startEffect);

	lua_newtable(L);
	luaL_register(L, nullptr, functions);
	lua_setfield(L, -2, "jsonGui");
}