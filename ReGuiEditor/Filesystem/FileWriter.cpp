#include "FileWriter.hpp" // VCIC-Excluded

#include "DirectoryManager.hpp"

#include <JSON/JSONObject.hpp>
#include <JSON/JSONWriter.hpp>

#include <UTILS/ByteStreamWriter.hpp>

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

bool Filesystem::writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
	auto resolvedPath = DirectoryManager::resolveDirectoryStatic(path);
	if (!resolvedPath)
		return false;

	if (!std::filesystem::exists(resolvedPath->parent_path())) {
		if (!std::filesystem::create_directories(resolvedPath->parent_path())) {
			CONSOLE_ERROR("Failed to create directories for path: %s", path.c_str());
			return false;
		}
	}

	HANDLE fileHandle = CreateFileW(resolvedPath->wstring().c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		CONSOLE_ERROR("Failed to open file for writing: %s", path.c_str());
		return false;
	}

	LARGE_INTEGER newSize;
	ZeroMemory(&newSize, sizeof(newSize));
	newSize.QuadPart = static_cast<LONGLONG>(data.size());

	if (!SetFilePointerEx(fileHandle, newSize, nullptr, FILE_BEGIN) || !SetEndOfFile(fileHandle)) {
		CONSOLE_ERROR("Failed to set file size: %s", path.c_str());

		CloseHandle(fileHandle);
		return false;
	}

	LARGE_INTEGER zeroOffset;
	ZeroMemory(&zeroOffset, sizeof(zeroOffset));
	if (!SetFilePointerEx(fileHandle, zeroOffset, nullptr, FILE_BEGIN)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to rewind file (Win32 error %lu): %s", GetLastError(), path.c_str());

		CloseHandle(fileHandle);
		return false;
	}

	HANDLE mappingHandle = CreateFileMappingA(fileHandle, nullptr, PAGE_READWRITE, 0, 0, nullptr);
	if (!mappingHandle) {
		CONSOLE_ERROR("Failed to create file mapping: %s", path.c_str());
	
		CloseHandle(fileHandle);
		return false;
	}

	LPVOID mappedView = MapViewOfFile(mappingHandle, FILE_MAP_WRITE, 0, 0, 0);
	if (!mappedView) {
		CONSOLE_ERROR("Failed to map view of file: %s", path.c_str());

		CloseHandle(mappingHandle);
		CloseHandle(fileHandle);
		return false;
	}

	memcpy(mappedView, data.data(), data.size());

	UnmapViewOfFile(mappedView);

	CloseHandle(mappingHandle);
	CloseHandle(fileHandle);
	return true;
}

bool Filesystem::writeFile(const std::filesystem::path& path, const std::string& data) {
	std::vector<uint8_t> byteData(data.begin(), data.end());
	return Filesystem::writeFile(path, byteData);
}

bool Filesystem::writeFile(const std::filesystem::path& path, const std::vector<char>& data) {
	std::vector<uint8_t> byteData(data.begin(), data.end());
	return Filesystem::writeFile(path, byteData);
}

bool Filesystem::writeFile(const std::filesystem::path& path, const UTILS::ByteStreamWriter& writer) {
	return Filesystem::writeFile(path, writer.toVector());
}

bool Filesystem::writeFile(const std::filesystem::path& path, const JSONValue& data, bool prettyPrint) {
	return Filesystem::writeFile(path, JSONWriter::writeJson(data, prettyPrint));
}
