#include "Console.hpp"
#include "Strings.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cwchar>
#include <limits>
#include <string>

static std::array<const wchar_t*, LEVEL_COUNT> gConsoleLogLevelNames = {
	L"[Default]",

	L"WARNING:",
	L"ERROR:",
	L"DEBUG:",

	L"[MyGUI]",
	L"[Shader]",
	L"[Render]"
};

inline static const wchar_t* consoleLogColorToAnsiEscapeCode(const ConsoleLogColor color) {
	switch (color) {
	case COLOR_BLACK: return L"\x1b[30m";
	case COLOR_DARK_RED: return L"\x1b[31m";
	case COLOR_DARK_GREEN: return L"\x1b[32m";
	case COLOR_DARK_YELLOW: return L"\x1b[33m";
	case COLOR_DARK_BLUE: return L"\x1b[34m";
	case COLOR_DARK_MAGENTA: return L"\x1b[35m";
	case COLOR_DARK_CYAN: return L"\x1b[36m";
	case COLOR_GRAY: return L"\x1b[37m";
	case COLOR_DARK_GRAY: return L"\x1b[90m";
	case COLOR_RED: return L"\x1b[91m";
	case COLOR_GREEN: return L"\x1b[92m";
	case COLOR_YELLOW: return L"\x1b[93m";
	case COLOR_BLUE: return L"\x1b[94m";
	case COLOR_MAGENTA: return L"\x1b[95m";
	case COLOR_CYAN: return L"\x1b[96m";
	case COLOR_WHITE: return L"\x1b[97m";
	default: return L"";
	}
}

inline static bool tryParseInteger(const std::wstring& str, int& outputValue, int base) {
	if (str.empty())
		return false;

	wchar_t* endPtr = nullptr;
	errno = 0;

	long value = std::wcstol(str.c_str(), &endPtr, base);
	if (errno == ERANGE || value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
		return false;

	if (*endPtr != L'\0')
		return false;

	outputValue = static_cast<int>(value);
	return true;
}

inline static bool tryParseInteger(const std::wstring& str, int& outputValue) {
	if (tryParseInteger(str, outputValue, 10))
		return true;

	if (tryParseInteger(str, outputValue, 16))
		return true;

	return false;
}

inline static void writeFile64(HANDLE handle, const wchar_t* buffer, size_t length) {
	if (length <= static_cast<size_t>(std::numeric_limits<DWORD>::max())) {
		DWORD written;
		WriteFile(handle, buffer, static_cast<DWORD>(length * sizeof(wchar_t)), &written, nullptr);

		return;
	}

	while (length > 0) {
		DWORD chunkSize = static_cast<DWORD>(std::min(length, static_cast<size_t>(std::numeric_limits<DWORD>::max())));

		DWORD written;
		WriteFile(handle, buffer, chunkSize * sizeof(wchar_t), &written, nullptr);

		buffer += chunkSize;
		length -= chunkSize;
	}
}

UTILS::Console::Console() {
	const LPWSTR commandLine = GetCommandLineW();

	int argc;
	LPWSTR* argv = CommandLineToArgvW(commandLine, &argc);
	if (argv == nullptr) {
		MessageBeep(MB_ICONERROR);
		ExitProcess(1);
		return;
	}

	for (int index = 1; index < argc; ++index) {
		std::wstring argument = argv[index];
		if (!argument.starts_with(L"--"))
			continue;

		this->arguments_.push_back(argument);

		if (index + 1 >= argc) {
			this->argumentsBools_.push_back(argument);
			break;
		}

		std::wstring nextArgument = argv[index + 1];
		if (nextArgument.starts_with(L"--")) {
			this->argumentsBools_.push_back(argument);
			continue;
		}

		++index;

		int intValue;
		if (tryParseInteger(nextArgument, intValue))
			this->argumentsInts_[argument] = intValue;
		else
			this->argumentsStrings_[argument] = nextArgument;
	}

	if (this->getArgumentBool(L"--console") && !GetConsoleWindow()) {
		if (!AllocConsole()) {
			MessageBox(NULL, L"Failed to allocate console.", L"Error", MB_ICONERROR);

			ExitProcess(1);
			return;
		}

		SetConsoleTitle(L"ReGuiEditor Console");
		SetConsoleCP(CP_UTF8);
		SetConsoleOutputCP(CP_UTF8);

		this->consoleOutputHandle_ = GetStdHandle(STD_OUTPUT_HANDLE);
		this->consoleErrorHandle_ = GetStdHandle(STD_ERROR_HANDLE);

		if (this->consoleOutputHandle_ == INVALID_HANDLE_VALUE || this->consoleErrorHandle_ == INVALID_HANDLE_VALUE) {
			MessageBox(NULL, L"Failed to get console handles.", L"Error", MB_ICONERROR);

			ExitProcess(1);
			return;
		}

		DWORD mode;
		GetConsoleMode(this->consoleOutputHandle_, &mode);
		mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(this->consoleOutputHandle_, mode);

		GetConsoleMode(this->consoleErrorHandle_, &mode);
		mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(this->consoleErrorHandle_, mode);

		this->consoleAllocated_ = true;
	}

	if (CreateDirectory(L"../Logs/", nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) {
		SYSTEMTIME systemTime;
		GetLocalTime(&systemTime);

		const std::wstring logFilename = UTILS::formatString(L"../Logs/ReGuiEditor_%04d-%02d-%02d_%02d-%02d-%02d.log", systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond);

		this->fileHandle_ = CreateFile(logFilename.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (this->fileHandle_ == INVALID_HANDLE_VALUE) {
			MessageBox(NULL, L"Failed to create log file.", L"Error", MB_ICONERROR);
			return;
		}

		const BYTE bom[] = { 0xFF, 0xFE };

		DWORD written;
		WriteFile(this->fileHandle_, bom, sizeof(bom), &written, nullptr);

		const std::wstring latestLinkPath = L"../Logs/ReGuiEditor_latest.log";
		DeleteFile(latestLinkPath.c_str());

		if (!CreateHardLink(latestLinkPath.c_str(), logFilename.c_str(), nullptr))
			this->log(LEVEL_WARNING, COLOR_YELLOW, L"Failed to create latest.log hard link.");
	}

	InitializeCriticalSection(&this->criticalSection_);

	this->log(LEVEL_DEFAULT, COLOR_GRAY, L"Console initialized.");
}

UTILS::Console::~Console() {
	this->log(LEVEL_DEFAULT, COLOR_GRAY, L"Console shutting down...");

	if (this->fileHandle_ != INVALID_HANDLE_VALUE) {
		CloseHandle(this->fileHandle_);
		this->fileHandle_ = INVALID_HANDLE_VALUE;
	}

	if (this->consoleAllocated_) {
		FreeConsole();
		this->consoleAllocated_ = false;
	}

	DeleteCriticalSection(&this->criticalSection_);
}

void UTILS::Console::log(ConsoleLogLevel level, ConsoleLogColor color, const std::wstring& message) {
	if (!this->consoleAllocated_ && this->fileHandle_ == INVALID_HANDLE_VALUE)
		return;


	SYSTEMTIME time;
	GetLocalTime(&time);

	const wchar_t* levelString = (level >= 0 && level < LEVEL_COUNT) ? gConsoleLogLevelNames[level] : L"[Unknown]";
	const std::wstring formattedMessage = UTILS::formatString(L"%02d:%02d:%02d %s %s\n", time.wHour, time.wMinute, time.wSecond, levelString, message.c_str());

	EnterCriticalSection(&this->criticalSection_);

	if (this->consoleAllocated_) {		
		const wchar_t* colorCode = consoleLogColorToAnsiEscapeCode(color);
		const HANDLE outputHandle = (level == LEVEL_ERROR) ? this->consoleErrorHandle_ : this->consoleOutputHandle_;
		writeFile64(outputHandle, colorCode, wcslen(colorCode));
		writeFile64(outputHandle, formattedMessage.c_str(), formattedMessage.size());
		writeFile64(outputHandle, L"\x1b[0m", 4);

	}

	if (this->fileHandle_ != INVALID_HANDLE_VALUE)
		writeFile64(this->fileHandle_, formattedMessage.c_str(), formattedMessage.size());

	LeaveCriticalSection(&this->criticalSection_);
}

bool UTILS::Console::hasArgument(const std::wstring& argument) const {
	return std::find(this->arguments_.begin(), this->arguments_.end(), argument) != this->arguments_.end();
}

bool UTILS::Console::getArgumentBool(const std::wstring& argument) const {
	return std::find(this->argumentsBools_.begin(), this->argumentsBools_.end(), argument) != this->argumentsBools_.end();
}

bool UTILS::Console::getArgumentInt(const std::wstring& argument, int& outputValue) const {
	auto iterator = this->argumentsInts_.find(argument);
	if (iterator != this->argumentsInts_.end()) {
		outputValue = iterator->second;
		return true;
	}

	return false;
}

bool UTILS::Console::getArgumentString(const std::wstring& argument, std::wstring& outputValue) const {
	auto iterator = this->argumentsStrings_.find(argument);
	if (iterator != this->argumentsStrings_.end()) {
		outputValue = iterator->second;
		return true;
	}

	return false;
}