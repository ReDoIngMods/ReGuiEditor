#pragma once
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

namespace Filesystem {
	[[nodiscard]] std::optional<std::vector<FilesystemListEntry>> listDirectory(const std::string& directory, bool includeFiles = true, bool includeDirectories = true);
}
