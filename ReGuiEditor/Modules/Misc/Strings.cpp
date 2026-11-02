#include "Strings.hpp" // VCIC-Excluded

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <string>

std::wstring ReGuiEditor::toWideString(const std::string& str) {
	int neededSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
	if (neededSize <= 0)
		return L"";

	std::wstring wideStr(neededSize, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wideStr[0], neededSize);

	return wideStr;
}

std::string ReGuiEditor::toShortString(const std::wstring& str) {
	int neededSize = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (neededSize <= 0)
		return "";

	std::string shortStr(neededSize, '\0');
	WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &shortStr[0], neededSize, NULL, NULL);

	return shortStr;
}

std::wstring ReGuiEditor::formatString(const std::wstring format, ...) {
	va_list args;
	va_start(args, format);

	const std::wstring result = ReGuiEditor::formatStringV(format, args);

	va_end(args);
	return result;
}

std::wstring ReGuiEditor::formatStringV(const std::wstring& format, va_list args) {
	va_list argsCopy;
	va_copy(argsCopy, args);

	int size = _vscwprintf(format.c_str(), argsCopy) + 1;
	
	va_end(argsCopy);

	if (size <= 0)
		return L"";

	std::wstring result(size, L'\0');
	vswprintf_s(&result[0], size, format.c_str(), args);

	result.resize(static_cast<size_t>(size) - 1); // Drop the null terminator
	return result;
}

std::string ReGuiEditor::formatString(const std::string format, ...) {
	va_list args;
	va_start(args, format);

	const std::string result = ReGuiEditor::formatStringV(format, args);

	va_end(args);
	return result;
}

std::string ReGuiEditor::formatStringV(const std::string& format, va_list args) {
	va_list argsCopy;
	va_copy(argsCopy, args);

	int size = _vscprintf(format.c_str(), argsCopy) + 1;

	va_end(argsCopy);

	if (size <= 0)
		return "";

	std::string result(size, '\0');
	vsnprintf_s(&result[0], size, size, format.c_str(), args);

	result.resize(static_cast<size_t>(size) - 1); // Drop the null terminator
	return result;
}