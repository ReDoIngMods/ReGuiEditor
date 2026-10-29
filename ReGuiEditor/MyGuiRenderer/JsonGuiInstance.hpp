#pragma once
#include "JsonGui/JsonGuiTypes.hpp"

#include <JSON/JSONObject.hpp>

#include <MyGui/MyGUI_EditBox.h>
#include <MyGui/MyGUI_MouseButton.h>
#include <MyGui/MyGUI_Widget.h>

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

class JsonGuiInstance {
	struct WidgetEvents {
		std::string name;
		std::array<std::string, static_cast<size_t>(GuiEventType::COUNT)> handlers;
	};

	GuiSettings settings_;

	MyGUI::Widget* root_ = nullptr;
	bool active_ = false;
	bool hidden_ = false;

	GuiEventSink eventSink_;
	std::unordered_map<MyGUI::Widget*, WidgetEvents> widgetEvents_;

	std::string closeHandler_;
	std::string rootName_;
	bool closeCallbackCleared_ = false;

	[[nodiscard]] MyGUI::Widget* build(const JSONValue& node, MyGUI::Widget* parent, int depth);
	[[nodiscard]] static bool loadSubLayout(const std::string& path, int depth, JSONValue& output);
	void destroyRoot();

	void bindEvent(MyGUI::Widget* widget, const std::string& name, std::string_view key, const JSONValue& value, bool isRoot);
	void queueEvent(MyGUI::Widget* sender, GuiEventType type, int x = 0, int y = 0, std::string text = {});

	void onMouseClick(MyGUI::Widget* sender);
	void onMousePressed(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
	void onMouseReleased(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
	void onMouseDrag(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id);
	void onMouseWheel(MyGUI::Widget* sender, int rel);

	void onEditTextChange(MyGUI::EditBox* sender);
	void onEditSelectAccept(MyGUI::EditBox* sender);
public:
	explicit JsonGuiInstance(GuiSettings settings) : settings_(std::move(settings)) { }
	~JsonGuiInstance() { this->destroyRoot(); }

	JsonGuiInstance(JsonGuiInstance&) = delete;
	JsonGuiInstance(JsonGuiInstance&&) = delete;
	JsonGuiInstance& operator=(JsonGuiInstance&) = delete;
	JsonGuiInstance& operator=(JsonGuiInstance&&) = delete;

	bool render(const JSONValue& document);
	void close();

	void closeByUser();

	void setHidden(bool hidden);

	void setEventSink(GuiEventSink sink) { this->eventSink_ = std::move(sink); }
	void clearOnCloseCallback() { this->closeCallbackCleared_ = true; this->closeHandler_.clear(); }

	[[nodiscard]] bool isActive() const { return this->active_; }
	[[nodiscard]] bool isHidden() const { return this->hidden_; }
	[[nodiscard]] const GuiSettings& getSettings() const { return this->settings_; }

	[[nodiscard]] MyGUI::Widget* findWidget(const std::string& name) const;

	static void dispatchPendingEvents();

	static void registerWidgetFactories();
	static void registerOverlayShader();
};
