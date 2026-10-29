#include "../JsonGuiInstance.hpp"

#include "JsonGuiParsing.hpp"
#include "JsonGuiTypes.hpp"

#include <JSON/JSONObject.hpp>

#include <MyGui/MyGUI_Delegate.h>
#include <MyGui/MyGUI_EditBox.h>
#include <MyGui/MyGUI_MouseButton.h>
#include <MyGui/MyGUI_Widget.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#pragma warning(disable: 6011)

struct PendingEvent {
	GuiEventSink sink;
	GuiEvent event;
};

[[nodiscard]] static std::vector<PendingEvent>& getPendingEvents() {
	static std::vector<PendingEvent> events;
	return events;
}

void JsonGuiInstance::bindEvent(MyGUI::Widget* widget, const std::string& name, std::string_view key, const JSONValue& value, bool isRoot) {
	const std::string* handler = value.tryGet<std::string>();
	if (!handler || handler->empty()) {
		CONSOLE_WARNING("JsonGui: event '%s' of widget '%s' must be the name of a Lua function.", std::string(key).c_str(), name.c_str());
		return;
	}

	if (key == "onClose") {
		if (!isRoot) {
			CONSOLE_WARNING("JsonGui: 'onClose' is only supported on the root widget (found on '%s').", name.c_str());
			return;
		}

		this->rootName_ = name;
		if (!this->closeCallbackCleared_)
			this->closeHandler_ = *handler;

		return;
	}

	GuiEventType type = GuiEventType::Click;
	if (!JsonGui::toEventType(key, type)) {
		CONSOLE_WARNING("JsonGui: event '%s' of widget '%s' is not supported.", std::string(key).c_str(), name.c_str());
		return;
	}

	MyGUI::EditBox* editBox = nullptr;
	if (type == GuiEventType::TextEdit || type == GuiEventType::TextEnter) {
		editBox = widget->castType<MyGUI::EditBox>(false);
		if (!editBox) {
			CONSOLE_WARNING("JsonGui: event '%s' of widget '%s' requires an EditBox.", std::string(key).c_str(), name.c_str());
			return;
		}
	}

	WidgetEvents& widgetEvents = this->widgetEvents_[widget];
	widgetEvents.name = name;
	widgetEvents.handlers[static_cast<size_t>(type)] = *handler;

	switch (type) {
	case GuiEventType::Click:
		widget->eventMouseButtonClick += MyGUI::newDelegate(this, &JsonGuiInstance::onMouseClick);
		break;
	case GuiEventType::Pressed:
		widget->eventMouseButtonPressed += MyGUI::newDelegate(this, &JsonGuiInstance::onMousePressed);
		break;
	case GuiEventType::Released:
		widget->eventMouseButtonReleased += MyGUI::newDelegate(this, &JsonGuiInstance::onMouseReleased);
		break;
	case GuiEventType::Drag:
		widget->eventMouseDrag += MyGUI::newDelegate(this, &JsonGuiInstance::onMouseDrag);
		break;
	case GuiEventType::MouseWheel:
		widget->eventMouseWheel += MyGUI::newDelegate(this, &JsonGuiInstance::onMouseWheel);
		break;
	case GuiEventType::TextEdit:
		editBox->eventEditTextChange += MyGUI::newDelegate(this, &JsonGuiInstance::onEditTextChange);
		break;
	case GuiEventType::TextEnter:
		editBox->eventEditSelectAccept += MyGUI::newDelegate(this, &JsonGuiInstance::onEditSelectAccept);
		break;
	default:
		break;
	}
}

void JsonGuiInstance::queueEvent(MyGUI::Widget* sender, GuiEventType type, int x, int y, std::string text) {
	if (!this->eventSink_)
		return;

	const auto iterator = this->widgetEvents_.find(sender);
	if (iterator == this->widgetEvents_.end())
		return;

	const std::string& handler = iterator->second.handlers[static_cast<size_t>(type)];
	if (handler.empty())
		return;

	GuiEvent event;
	event.type = type;
	event.handler = handler;
	event.widget = iterator->second.name;
	event.text = std::move(text);
	event.x = x;
	event.y = y;

	getPendingEvents().emplace_back(this->eventSink_, std::move(event));
}

void JsonGuiInstance::onMouseClick(MyGUI::Widget* sender) {
	this->queueEvent(sender, GuiEventType::Click);
}

void JsonGuiInstance::onMousePressed(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id) {
	if (id == MyGUI::MouseButton::Left)
		this->queueEvent(sender, GuiEventType::Pressed, left, top);
}

void JsonGuiInstance::onMouseReleased(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id) {
	if (id == MyGUI::MouseButton::Left)
		this->queueEvent(sender, GuiEventType::Released, left, top);
}

void JsonGuiInstance::onMouseDrag(MyGUI::Widget* sender, int left, int top, MyGUI::MouseButton id) {
	if (id == MyGUI::MouseButton::Left)
		this->queueEvent(sender, GuiEventType::Drag, left, top);
}

void JsonGuiInstance::onMouseWheel(MyGUI::Widget* sender, int rel) {
	this->queueEvent(sender, GuiEventType::MouseWheel, rel);
}

void JsonGuiInstance::onEditTextChange(MyGUI::EditBox* sender) {
	this->queueEvent(sender, GuiEventType::TextEdit, 0, 0, sender->getOnlyText().asUTF8());
}

void JsonGuiInstance::onEditSelectAccept(MyGUI::EditBox* sender) {
	this->queueEvent(sender, GuiEventType::TextEnter, 0, 0, sender->getOnlyText().asUTF8());
}

void JsonGuiInstance::closeByUser() {
	if (!this->active_)
		return;

	if (this->eventSink_ && !this->closeHandler_.empty()) {
		GuiEvent event;
		event.type = GuiEventType::Close;
		event.handler = this->closeHandler_;
		event.widget = this->rootName_;

		getPendingEvents().emplace_back(this->eventSink_, std::move(event));
	}

	this->close();
}

void JsonGuiInstance::dispatchPendingEvents() {
	std::vector<PendingEvent> events;
	events.swap(getPendingEvents());

	for (const PendingEvent& pending : events)
		pending.sink(pending.event);
}
