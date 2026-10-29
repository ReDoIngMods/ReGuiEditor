#pragma once
#include <functional>
#include <string>

enum class GuiEventType : size_t {
	Click,
	Pressed,
	Released,
	Drag,
	MouseWheel,
	TextEdit,
	TextEnter,
	Close,

	COUNT
};

struct GuiEvent {
	GuiEventType type = GuiEventType::Click;
	std::string handler;

	std::string widget;
	std::string text;

	int x = 0;
	int y = 0;
};

using GuiEventSink = std::function<void(const GuiEvent&)>;

struct GuiSettings {
	bool isHud = false;
	bool isInteractive = true;
	bool needsCursor = true;
	bool hidesHotbar = false;
	bool isOverlapped = false;
	bool openCloseAudio = true;
	float backgroundAlpha = 0.6f;

	std::string handleKeySetup;
	std::string name = "Default";
	std::string layer = "Middle";
};
