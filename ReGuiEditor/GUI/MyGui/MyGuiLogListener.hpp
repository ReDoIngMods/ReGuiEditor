#pragma once
#include <MyGui/MyGUI_ILogListener.h>
#include <MyGui/MyGUI_LogLevel.h>

#include <ctime>
#include <string>
#include <filesystem>

class MyGuiLogListener : public MyGUI::ILogListener {
public:
	void log(const std::string& _section, MyGUI::LogLevel _level, const struct tm* _time, const std::string& _message, const char* _file, int _line) override {
		const std::filesystem::path filePath(_file);
		const std::string fileName = filePath.filename().string();

		if (_level == MyGUI::LogLevel::Info)
			//CONSOLE_LOG(LEVEL_MYGUI, COLOR_DARK_GRAY, "INFO  | %-8s | %s:%d %s", _section.c_str(), fileName.c_str(), _line, _message.c_str());
			do {} while (0); // Ignore info messages to reduce console spam
		else if (_level == MyGUI::LogLevel::Warning)
			CONSOLE_LOG(LEVEL_MYGUI, COLOR_YELLOW, "WARN  | %-8s | %s:%d %s", _section.c_str(), fileName.c_str(), _line, _message.c_str());
		else if (_level == MyGUI::LogLevel::Error)
			CONSOLE_LOG(LEVEL_MYGUI, COLOR_RED, "ERROR | %-8s | %s:%d %s", _section.c_str(), fileName.c_str(), _line, _message.c_str());
		else if (_level == MyGUI::LogLevel::Critical)
			CONSOLE_LOG(LEVEL_MYGUI, COLOR_RED, "FATAL | %-8s | %s:%d %s", _section.c_str(), fileName.c_str(), _line, _message.c_str());
		else
			CONSOLE_LOG(LEVEL_MYGUI, COLOR_MAGENTA, "????  | %-8s | %s:%d %s", _section.c_str(), fileName.c_str(), _line, _message.c_str());
	};
};