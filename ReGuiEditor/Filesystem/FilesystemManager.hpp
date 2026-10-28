#pragma once
#include <UTILS/Singleton.hpp>

#include <UTILS/ByteStreamReader.hpp>
#include <UTILS/ByteStreamWriter.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct FilesystemListEntry {
	std::string name;
	std::string formattedWriteTime = "N/A";
	std::string fileTypename = "N/A";
	
	time_t writeTime = 0;
	size_t fileSize = 0;

	bool isDirectory = false;
};

class FilesystemManager : public UTILS::Singleton<FilesystemManager> {
public:
	[[nodiscard]] bool readFile(const std::string& path, std::vector<uint8_t>& outputData) const;
	[[nodiscard]] bool readFile(const std::string& path, std::string& outputString) const {
		std::vector<uint8_t> data;
		if (!this->readFile(path, data))
			return false;

		outputString.assign(data.begin(), data.end());
		return true;
	}

	[[nodiscard]] bool readFile(const std::string& path, std::vector<char>& outputData) const {
		std::vector<uint8_t> data;
		if (!this->readFile(path, data))
			return false;

		outputData.assign(data.begin(), data.end());
		return true;
	}

	[[nodiscard]] bool readFile(const std::string& path, UTILS::ByteStreamReader& reader) const {
		std::vector<uint8_t> data;
		if (!this->readFile(path, data))
			return false;

		reader.setData(data.data(), data.size());
		return true;
	}

	[[nodiscard]] bool writeFile(const std::string& path, const std::vector<uint8_t>& data) const;
	[[nodiscard]] bool writeFile(const std::string& path, const std::string& data) const {
		std::vector<uint8_t> byteData(data.begin(), data.end());
		return this->writeFile(path, byteData);
	}

	[[nodiscard]] bool writeFile(const std::string& path, const std::vector<char>& data) const {
		std::vector<uint8_t> byteData(data.begin(), data.end());
		return this->writeFile(path, byteData);
	}

	[[nodiscard]] bool writeFile(const std::string& path, const UTILS::ByteStreamWriter& writer) const {
		return this->writeFile(path, writer.toVector());
	}

	[[nodiscard]] bool exists(const std::string& path) const;
	[[nodiscard]] bool getFileWriteTime(const std::string& path, time_t& writeTime) const;

	[[nodiscard]] std::optional<std::vector<FilesystemListEntry>> listDirectory(const std::string& directory, bool includeFiles = true, bool includeDirectories = true) const;
};