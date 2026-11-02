#pragma once
#include <Modules/Bytestream/BytestreamReader.hpp>
#include <Modules/Bytestream/BytestreamWriter.hpp>
#include <Modules/JSON/JSONObject.hpp>
#include <Modules/Misc/Singleton.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ReGuiEditor {
	struct FilesystemListEntry {
		std::string name;
		std::string formattedWriteTime = "N/A";
		std::string fileTypename = "N/A";

		time_t writeTime = 0;
		size_t fileSize = 0;

		bool isDirectory = false;
	};

	class FilesystemManager : public ReGuiEditor::Singleton<FilesystemManager> {
	public:
		[[nodiscard]] bool readFile(const std::string& path, std::vector<uint8_t>& outputData) const;
		[[nodiscard]] bool readFile(const std::string& path, std::string& outputString) const;

		template<typename T>
		[[nodiscard]] inline bool readFile(const std::string& path, std::vector<T>& outputData) const {
			std::vector<uint8_t> data;
			if (!this->readFile(path, data))
				return false;

			outputData.assign(reinterpret_cast<const T*>(data.data()), reinterpret_cast<const T*>(data.data() + data.size()));
			return true;
		}

		[[nodiscard]] bool readFile(const std::string& path, ReGuiEditor::BytestreamReader& reader) const;
		[[nodiscard]] bool readFile(const std::string& path, ReGuiEditor::JSONValue& output) const;

		[[nodiscard]] bool writeFile(const std::string& path, const std::vector<uint8_t>& data) const;
		[[nodiscard]] bool writeFile(const std::string& path, const std::string& data) const;

		template<typename T>
		[[nodiscard]] inline bool writeFile(const std::string& path, const std::vector<T>& data) const {
			const std::vector<uint8_t> byteData(reinterpret_cast<const uint8_t*>(data.data()), reinterpret_cast<const uint8_t*>(data.data() + data.size()));
			return this->writeFile(path, byteData);
		}

		[[nodiscard]] bool writeFile(const std::string& path, const ReGuiEditor::BytestreamWriter& writer) const;
		[[nodiscard]] bool writeFile(const std::string& path, const ReGuiEditor::JSONValue& data, bool prettyPrint = true) const;

		[[nodiscard]] bool exists(const std::string& path) const;
		[[nodiscard]] bool getFileWriteTime(const std::string& path, time_t& writeTime) const;

		[[nodiscard]] std::optional<std::vector<FilesystemListEntry>> listDirectory(const std::string& directory, bool includeFiles = true, bool includeDirectories = true) const;
	};
}