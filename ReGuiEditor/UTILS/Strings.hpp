#pragma once
#include <cstdio>
#include <string>

namespace UTILS {
	std::wstring toWideString(const std::string& str);
	std::string toShortString(const std::wstring& str);

	template<typename ...Arguments>
	std::wstring formatString(const std::wstring& format, Arguments... args) {
		int size = swprintf(nullptr, 0, format.c_str(), args...) + 1;
		if (size <= 0)
			return L"";

		std::wstring result(size, L'\0');
		swprintf(&result[0], size, format.c_str(), args...);

		result.resize(static_cast<size_t>(size) - 1);
		return result;
	}

	template<typename ...Arguments>
	std::string formatString(const std::string& format, Arguments... args) {
		int size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
		if (size <= 0)
			return "";

		std::string result(size, '\0');
		snprintf(&result[0], size, format.c_str(), args...);

		result.resize(static_cast<size_t>(size) - 1);
		return result;
	}
}