#include "FileReader.hpp" // VCIC-Excluded

#include "DirectoryManager.hpp"

#include <JSON/JSONObject.hpp>
#include <JSON/JSONParser.hpp>

#include <UTILS/UnicodeDecoder.hpp>

#include <Windows.h>

#include <UTILS/Strings.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <vector>

inline static bool startsWith(const std::span<const uint8_t>& data, std::initializer_list<uint8_t> prefix) {
	if (data.size() < prefix.size())
		return false;

	return memcmp(data.data(), prefix.begin(), prefix.size()) == 0;
}

std::optional<std::vector<uint8_t>> Filesystem::readFileBinary(const std::filesystem::path& path) {
	auto resolvedPath = DirectoryManager::resolveDirectoryStatic(path);
	if (!resolvedPath)
		return std::nullopt;
	
	if (!std::filesystem::exists(resolvedPath.value()))
		return std::nullopt;

	HANDLE fileHandle = CreateFileA(resolvedPath->string().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		CONSOLE_ERROR("Failed to open file for reading: %s", path.string().c_str());
		return std::nullopt;
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(fileHandle, &fileSize)) {
		CONSOLE_ERROR("Failed to get file size: %s", path.string().c_str());
	
		CloseHandle(fileHandle);
		return std::nullopt;
	}

	HANDLE mappingHandle = CreateFileMappingA(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (!mappingHandle) {
		CONSOLE_ERROR("Failed to create file mapping: %s", path.string().c_str());

		CloseHandle(fileHandle);
		return std::nullopt;
	}

	LPVOID mappedView = MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, 0);
	if (!mappedView) {
		CONSOLE_ERROR("Failed to map view of file: %s", path.string().c_str());
		
		CloseHandle(mappingHandle);
		CloseHandle(fileHandle);
		return std::nullopt;
	}

	std::vector<uint8_t> outputData(static_cast<size_t>(fileSize.QuadPart));
	memcpy(outputData.data(), mappedView, static_cast<size_t>(fileSize.QuadPart));

	UnmapViewOfFile(mappedView);

	CloseHandle(mappingHandle);
	CloseHandle(fileHandle);

	return outputData;
}

std::optional<std::wstring> Filesystem::readFileText(const std::filesystem::path& path) {
	auto data = Filesystem::readFileBinary(path);
	if (!data)
		return std::nullopt;

	const std::span<const uint8_t> dataSpan(data->data(), data->size());
	if (startsWith(dataSpan, { 0xFF, 0xFE, 0x00, 0x00 })) return UTILS::UnicodeDecoder::fromUTF32(dataSpan.subspan(4), true);
	if (startsWith(dataSpan, { 0x00, 0x00, 0xFE, 0xFF })) return UTILS::UnicodeDecoder::fromUTF32(dataSpan.subspan(4), false);

	if (startsWith(dataSpan, { 0xEF, 0xBB, 0xBF })) return UTILS::UnicodeDecoder::fromUTF8(dataSpan.subspan(3));
	
	if (startsWith(dataSpan, { 0xFF, 0xFE })) return UTILS::UnicodeDecoder::fromUTF16(dataSpan.subspan(2), true);
	if (startsWith(dataSpan, { 0xFE, 0xFF })) return UTILS::UnicodeDecoder::fromUTF16(dataSpan.subspan(2), false);

	// No BOM detected, assume UTF-8 by default

	return UTILS::UnicodeDecoder::fromUTF8(dataSpan);
}

std::optional<std::string> Filesystem::readFileTextUTF8(const std::filesystem::path& path) {
	auto text = Filesystem::readFileText(path);
	if (!text)
		return std::nullopt;

	return UTILS::toShortString(text.value());
}

std::optional<JSONValue> Filesystem::readFileJson(const std::filesystem::path& path) {
	auto text = Filesystem::readFileTextUTF8(path);
	if (!text)
		return std::nullopt;

	return JSONParser::parseJson(text.value());
}