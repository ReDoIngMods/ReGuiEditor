#include "Filesystem.hpp" // VCIC-Excluded: Includer cleaner is being a dum dum
#include <filesystem>
#include <string>

#include <Windows.h>
#include <cstring>

bool FS::createDirectories(const std::filesystem::path& path) {
	if (path.empty())
		return false;

	const std::wstring wpath = path.wstring();

	DWORD attributes = GetFileAttributesW(wpath.c_str());
	if (attributes != INVALID_FILE_ATTRIBUTES)
		return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	const auto parent = path.parent_path();
	if (!parent.empty() && parent != path) {
		if (!FS::createDirectories(parent))
			return false;
	}

	if (CreateDirectoryW(wpath.c_str(), nullptr))
		return true;

	const DWORD error = GetLastError();
	if (error == ERROR_ALREADY_EXISTS) {
		attributes = GetFileAttributesW(wpath.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create directory: %s (error: %lu)", path.string().c_str(), error);
	return false;
}

bool FS::readFile(const std::filesystem::path& path, std::string& outContent) {
	if (!FS::fileExists(path))
		return false;

	FS::createDirectories(path.parent_path());
	
	HANDLE fileHandle = CreateFileW(path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to open file for reading: %s", path.string().c_str());
		return false;
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(fileHandle, &fileSize)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get file size for reading: %s", path.string().c_str());

		CloseHandle(fileHandle);
		return false;
	}

	HANDLE fileMapping = CreateFileMappingW(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (!fileMapping) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create file mapping for reading: %s", path.string().c_str());
		
		CloseHandle(fileHandle);
		return false;
	}

	LPVOID mappedView = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
	if (!mappedView) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to map view of file for reading: %s", path.string().c_str());
		
		CloseHandle(fileMapping);
		CloseHandle(fileHandle);
		return false;
	}

	outContent.assign(static_cast<const char*>(mappedView), static_cast<size_t>(fileSize.QuadPart));

	UnmapViewOfFile(mappedView);
	CloseHandle(fileMapping);
	CloseHandle(fileHandle);

	return true;
}

bool FS::writeFile(const std::filesystem::path& path, const std::string& content) {
	FS::createDirectories(path.parent_path());

	HANDLE fileHandle = CreateFileW(path.wstring().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to open file for writing: %s", path.string().c_str());
		return false;
	}

	if (content.empty()) {
		CloseHandle(fileHandle);
		return true;
	}

	LARGE_INTEGER newSize{};
	newSize.QuadPart = static_cast<LONGLONG>(content.size());

	if (!SetFilePointerEx(fileHandle, newSize, nullptr, FILE_BEGIN) || !SetEndOfFile(fileHandle)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to resize file for writing: %s", path.string().c_str());
		
		CloseHandle(fileHandle);
		return false;
	}

	HANDLE fileMapping = CreateFileMappingW(fileHandle, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(newSize.QuadPart), nullptr);
	if (!fileMapping) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create file mapping for writing: %s", path.string().c_str());

		CloseHandle(fileHandle);
		return false;
	}

	LPVOID mappedView = MapViewOfFile(fileMapping, FILE_MAP_WRITE, 0, 0, 0);
	if (!mappedView) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to map view of file for writing: %s", path.string().c_str());
		
		CloseHandle(fileMapping);
		CloseHandle(fileHandle);
		return false;
	}

	memcpy(mappedView, content.c_str(), content.size());
	
	UnmapViewOfFile(mappedView);
	CloseHandle(fileMapping);
	CloseHandle(fileHandle);

	return true;
}

bool FS::fileExists(const std::filesystem::path& path) {
	DWORD attributes = GetFileAttributesW(path.wstring().c_str());
	return (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool FS::directoryExists(const std::filesystem::path& path) {
	DWORD attributes = GetFileAttributesW(path.wstring().c_str());
	return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool FS::fileOrDirectoryExists(const std::filesystem::path& path) {
	DWORD attributes = GetFileAttributesW(path.wstring().c_str());
	return (attributes != INVALID_FILE_ATTRIBUTES);
}

bool FS::getFileWriteTime(const std::filesystem::path& path, std::filesystem::file_time_type& writeTime) {
	if (!FS::fileExists(path))
		return false;

	HANDLE fileHandle = CreateFileW(path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to open file to get write time: %s", path.string().c_str());
		return false;
	}

	FILETIME ftWrite;
	if (!GetFileTime(fileHandle, nullptr, nullptr, &ftWrite)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to get file write time: %s", path.string().c_str());

		CloseHandle(fileHandle);
		return false;
	}

	CloseHandle(fileHandle);

	return true;
}