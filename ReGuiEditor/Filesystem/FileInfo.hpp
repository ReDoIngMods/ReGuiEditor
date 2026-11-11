#pragma once
#include <filesystem>

namespace Filesystem {
	[[nodiscard]] bool exists(const std::filesystem::path& path);
	[[nodiscard]] bool getFileWriteTime(const std::filesystem::path& path, time_t& writeTime);
}
