#pragma once
#include <JSON/JSON.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Filesystem {
	[[nodiscard]] std::optional<std::vector<uint8_t>> readFileBinary(const std::filesystem::path& path);
	[[nodiscard]] std::optional<std::wstring> readFileText(const std::filesystem::path& path);
	[[nodiscard]] std::optional<std::string> readFileTextUTF8(const std::filesystem::path& path);

	[[nodiscard]] std::optional<JSONValue> readFileJson(const std::filesystem::path& path);
}
