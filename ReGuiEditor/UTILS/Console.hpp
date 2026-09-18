#pragma once
#if defined(__cplusplus)
#include "Strings.hpp"

#include <Windows.h>

#include <string>
#include <unordered_map>
#include <vector>

enum ConsoleLogLevel {
	LEVEL_DEFAULT,

	LEVEL_WARNING,
	LEVEL_ERROR,
	LEVEL_DEBUG,

	LEVEL_CACHE,

	LEVEL_MYGUI,
	LEVEL_SHADER,
	LEVEL_RENDER,

	LEVEL_COUNT
};

enum ConsoleLogColor {
	COLOR_BLACK,
	COLOR_DARK_RED,
	COLOR_DARK_GREEN,
	COLOR_DARK_YELLOW,
	COLOR_DARK_BLUE ,
	COLOR_DARK_MAGENTA,
	COLOR_DARK_CYAN,
	COLOR_GRAY,
	COLOR_DARK_GRAY,
	COLOR_RED,
	COLOR_GREEN,
	COLOR_YELLOW,
	COLOR_BLUE,
	COLOR_MAGENTA,
	COLOR_CYAN,
	COLOR_WHITE
};

namespace UTILS {
	class Console {
		bool consoleAllocated_ = false;

		HANDLE fileHandle_ = INVALID_HANDLE_VALUE;

		HANDLE consoleOutputHandle_ = nullptr;
		HANDLE consoleErrorHandle_ = nullptr;

		CRITICAL_SECTION criticalSection_;

		std::unordered_map<std::wstring, std::wstring> argumentsStrings_;
		std::unordered_map<std::wstring, int> argumentsInts_;
		std::vector<std::wstring> argumentsBools_;
		std::vector<std::wstring> arguments_;

		void createLogFile();
		void parseArguments();
		void createConsole();
	public:
		Console();
		~Console();

		static Console& getInstance() {
			static Console instance;
			return instance;
		}

		void log(ConsoleLogLevel level, ConsoleLogColor color, const std::wstring& message);

		void log(ConsoleLogLevel level, ConsoleLogColor color, const std::string& message) {
			std::wstring wideMessage = UTILS::toWideString(message);

			this->log(level, color, wideMessage);
		}

		template<typename ...Arguments>
		void log(ConsoleLogLevel level, ConsoleLogColor color, const std::wstring& format, Arguments... args) {
			std::wstring message = UTILS::formatString(format, args...);

			this->log(level, color, message);
		}

		template<typename ...Arguments>
		void log(ConsoleLogLevel level, ConsoleLogColor color, const std::string& format, Arguments... args) {
			std::string message = UTILS::formatString(format, args...);
			std::wstring wideMessage = UTILS::toWideString(message);

			this->log(level, color, wideMessage);
		}

		bool hasArgument(const std::wstring& argument) const;
		bool getArgumentBool(const std::wstring& argument) const;
		bool getArgumentInt(const std::wstring& argument, int& outputValue) const;
		bool getArgumentString(const std::wstring& argument, std::wstring& outputValue) const;
	};
}

#define CONSOLE_LOG UTILS::Console::getInstance().log

#endif