#pragma once
#include <UTILS/ByteStreamWriter.hpp>

#include <JSON/JSON.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Filesystem {
	[[nodiscard]] bool writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& data);
	[[nodiscard]] bool writeFile(const std::filesystem::path& path, const std::string& data);
	[[nodiscard]] bool writeFile(const std::filesystem::path& path, const std::vector<char>& data);
	[[nodiscard]] bool writeFile(const std::filesystem::path& path, const UTILS::ByteStreamWriter& writer);
	[[nodiscard]] bool writeFile(const std::filesystem::path& path, const JSONValue& data, bool prettyPrint = true, const char indentCharacter = '\t');
}
