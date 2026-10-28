#include "UGCMod.hpp"

#include <JSON/JSONObject.hpp>
#include <JSON/JSONParser.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <string>

std::array<std::string, static_cast<size_t>(UGCModType::COUNT) + 1> gUGCModTypeMap = {
	"Blueprint",
	"Blocks and Parts",
	"Tile",
	"World",
	"Terrain Assets",
	"Challenge Pack",
	"Challenge Level",
	"Custom Game",

	"Unknown"
};

const std::string& UGCModTypeToString(UGCModType type) {
	const size_t index = static_cast<size_t>(type);
	if (index >= gUGCModTypeMap.size())
		return gUGCModTypeMap.back();

	return gUGCModTypeMap[index];
}

UGCModType StringToUGCModType(const std::string& typeStr) {
	const auto iterator = std::find(gUGCModTypeMap.begin(), gUGCModTypeMap.end(), typeStr);
	if (iterator == gUGCModTypeMap.end())
		return UGCModType::COUNT;

	return static_cast<UGCModType>(std::distance(gUGCModTypeMap.begin(), iterator));
}

inline static std::string readFile(const std::filesystem::path& filePath) {
	std::ifstream file(filePath);
	if (!file) {
		CONSOLE_ERROR("Failed to read file: %s", filePath.string().c_str());
		return "";
	}

	std::string data;
	data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

	return data;
}

inline static std::string findThumbnailFilename(const std::filesystem::path& modPath) {
	for (const auto& entry : std::filesystem::directory_iterator(modPath)) {
		if (!entry.is_regular_file())
			continue;
	
		const std::string filename = entry.path().filename().string();
		if (filename.rfind("preview", 0) == 0)
			return filename;
	}

	return "";
}

std::shared_ptr<UGCMod> UGCMod::fromPath(const std::filesystem::path& modPath, bool isLocal) {
	const std::string descriptionPath = (modPath / "description.json").string();
	const std::string descriptionData = readFile(descriptionPath);
	if (descriptionData.empty()) {
		CONSOLE_ERROR("Failed to read description.json from %s", modPath.string().c_str());
		return nullptr;
	}

	JSONValue json;
	if (!JSONParser::parseJson(descriptionData, json)) {
		CONSOLE_ERROR("Failed to parse description.json from %s", modPath.string().c_str());
		return nullptr;
	}

	const std::string* namePtr = json["name"].tryGet<std::string>();
	if (!namePtr || namePtr->empty()) {
		CONSOLE_ERROR("description.json from %s does not contain a valid name.", modPath.string().c_str());
		return nullptr;
	}

	const std::string* localIdPtr = json["localId"].tryGet<std::string>();
	if (!localIdPtr || localIdPtr->empty()) {
		CONSOLE_ERROR("description.json from %s does not contain a valid localId.", modPath.string().c_str());
		return nullptr;
	}

	// On old versions of Scrap Mechanic, the "type" field is missing as all workshop items were blueprints. We have to do
	// it a little more differently to support older versions of Scrap Mechanic.
	UGCModType type = UGCModType::Blueprint;
	
	const std::string* typePtr = json["type"].tryGet<std::string>();
	if (typePtr) {
		type = StringToUGCModType(*typePtr);
	
		if (type == UGCModType::COUNT) {
			CONSOLE_ERROR("description.json from %s contains an unknown type: %s", modPath.string().c_str(), typePtr->c_str());
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

	(void)mod->getThumbnail(); // Force loads it incase this mod gets loaded when ImageManager IS avaliable.

	return mod;
}