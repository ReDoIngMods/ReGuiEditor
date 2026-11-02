#include "UGCMod.hpp"

#include <Modules/JSON/JSONObject.hpp>
#include <Modules/JSON/JSONParser.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <system_error>

constexpr auto gUGCModTypeMap = std::to_array<const char*>({
	"Blueprint",
	"Blocks and Parts",
	"Tile",
	"World",
	"Terrain Assets",
	"Challenge Pack",
	"Challenge Level",
	"Custom Game",
	"Unknown"
});

const char* UGCModTypeToString(UGCModType type) {
	const size_t index = static_cast<size_t>(type);
	if (index >= gUGCModTypeMap.size())
		return gUGCModTypeMap.back();

	return gUGCModTypeMap[index];
}

UGCModType StringToUGCModType(const char* typeStr) {
	for (size_t index = 0; index < gUGCModTypeMap.size(); ++index) {
		if (strcmp(gUGCModTypeMap[index], typeStr) == 0)
			return static_cast<UGCModType>(index);
	}

	return UGCModType::COUNT;
}

inline static bool readFile(const std::filesystem::path& filePath, std::string& output) {
	std::ifstream file(filePath, std::ios::binary | std::ios::ate);
	if (!file)
		return false;

	const std::streamsize size = file.tellg();
	if (size <= 0)
		return false;

	output.resize(static_cast<size_t>(size));
	file.seekg(0, std::ios::beg);

	return static_cast<bool>(file.read(output.data(), size));
}

inline static std::string findThumbnailFilename(const std::filesystem::path& modPath) {
	std::error_code error;
	for (std::filesystem::directory_iterator iterator(modPath, error), end; !error && iterator != end; iterator.increment(error)) {
		if (!iterator->is_regular_file(error))
			continue;

		const std::string filename = iterator->path().filename().string();
		if (filename.starts_with("preview"))
			return filename;
	}

	return "";
}

std::shared_ptr<UGCMod> UGCMod::fromPath(const std::filesystem::path& modPath, bool isLocal) {
	std::string error;
	std::shared_ptr<UGCMod> mod = fromPath(modPath, isLocal, error);
	if (!mod)
		CONSOLE_ERROR("%s", error.c_str());

	return mod;
}

std::shared_ptr<UGCMod> UGCMod::fromPath(const std::filesystem::path& modPath, bool isLocal, std::string& outputErrorMsg) {
	const std::string pathString = modPath.string();

	std::string descriptionData;
	if (!readFile(modPath / "description.json", descriptionData)) {
		outputErrorMsg = "Failed to read description.json";
		return nullptr;
	}

	ReGuiEditor::JSONValue json;
	if (!ReGuiEditor::JSONParser::parseJson(descriptionData, json)) {
		outputErrorMsg = "Failed to parse description.json";
		return nullptr;
	}

	const std::string* namePtr = json["name"].tryGet<std::string>();
	if (!namePtr || namePtr->empty()) {
		outputErrorMsg = "description.json does not contain a valid name.";
		return nullptr;
	}

	const std::string* localIdPtr = json["localId"].tryGet<std::string>();
	if (!localIdPtr || localIdPtr->empty()) {
		outputErrorMsg = "description.json does not contain a valid localId.";
		return nullptr;
	}

	// On old versions of Scrap Mechanic, the "type" field is missing as all workshop items were blueprints. We have to do
	// it a little more differently to support older versions of Scrap Mechanic.
	UGCModType type = UGCModType::Blueprint;
	
	const std::string* typePtr = json["type"].tryGet<std::string>();
	if (typePtr) {
		type = StringToUGCModType(typePtr->c_str());
	
		if (type == UGCModType::COUNT) {
			outputErrorMsg = "description.json does not contain a valid type: " + *typePtr;
			return nullptr;
		}
	}

	std::string thumbnailFilepath = findThumbnailFilename(modPath);
	if (thumbnailFilepath.empty())
		thumbnailFilepath = "$RGE_DATA/Images/thumbnail_missing.jpg";
	else
		thumbnailFilepath = "$CONTENT_" + *localIdPtr + "/" + thumbnailFilepath;

	std::shared_ptr<UGCMod> mod = std::make_shared<UGCMod>();
	mod->name = *namePtr;
	mod->localId = *localIdPtr;
	mod->thumbnailFilepath = thumbnailFilepath;
	mod->contentPath = "$CONTENT_" + *localIdPtr;
	mod->path = modPath;
	mod->fileId = json["fileId"].getOrDefault<uint64_t>(0);
	mod->type = type;
	mod->isLocal = isLocal;
	mod->version = json["version"].getOrDefault<int>(0);

	return mod;
}