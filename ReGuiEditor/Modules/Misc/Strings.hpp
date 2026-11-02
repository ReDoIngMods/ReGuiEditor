#pragma once
#include <cstdarg>
#include <string>

namespace ReGuiEditor {
	std::wstring toWideString(const std::string& str);
	std::string toShortString(const std::wstring& str);

	std::wstring formatString(const std::wstring format, ...);
	std::wstring formatStringV(const std::wstring&format, va_list args);

	std::string formatString(const std::string format, ...);
	std::string formatStringV(const std::string& format, va_list args);
}