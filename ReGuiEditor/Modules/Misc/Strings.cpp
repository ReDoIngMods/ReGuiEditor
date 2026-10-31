#include "Strings.hpp" // VCIC-Excluded

#include <Windows.h>

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