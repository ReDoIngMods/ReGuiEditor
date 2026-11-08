#include "UGCMod.hpp"

#include <JSON/JSONObject.hpp>
#include <JSON/JSONParser.hpp>

#include <UTILS/Uuid.hpp>

#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <system_error>

inline static bool readFileDirectly(const std::filesystem::path& filePath, std::string& output) {
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

inline static std::string findThumbnailFilenameDirectly(const std::filesystem::path& modPath, const std::string& modName) {
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(modPath, error)) {
		if (error)
			continue;

		if (!entry.is_regular_file(error))
			continue;

		const std::string filename = entry.path().filename().string();
		if (filename.starts_with("preview") || filename.starts_with("icon") || filename.starts_with(modName))
			return filename;
	}

	return "$RGE_DATA/Images/thumbnail_missing.jpg";
}

std::shared_ptr<UgcMod> UgcMod::createFromPath(std::filesystem::path modPath, bool isLocal) {
	modPath.make_preferred();

	std::string descriptionData;
	if (!readFileDirectly(modPath / "description.json", descriptionData)) {
		CONSOLE_ERROR("Failed to read description.json for mod at path '%s'.", modPath.string().c_str());
		return nullptr;
	}

	JSONValue json;
	if (!JSONParser::parseJson(descriptionData, json)) {
		CONSOLE_ERROR("Failed to parse description.json for mod at path '%s'.", modPath.string().c_str());
		return nullptr;
	}

	const std::string* name = json["name"].tryGet<std::string>();
	if (!name) {
		CONSOLE_ERROR("description.json is missing the 'name' field for mod at path '%s'.", modPath.string().c_str());
		return nullptr;
	}

	const std::string* localIdPtr = json["localId"].tryGet<std::string>();
	if (!localIdPtr) {
		CONSOLE_ERROR("description.json is missing the 'localId' field for mod at path '%s'.", modPath.string().c_str());
		return nullptr;
	}

	UgcModType type = UgcModType::Blueprint;

	const std::string* typeStr = json["type"].tryGet<std::string>();
	if (typeStr) {
		type = stringToUgcModType(typeStr->c_str());

		if (type == UgcModType::COUNT) {
			CONSOLE_ERROR("description.json has an invalid 'type' field for mod at path '%s'.", modPath.string().c_str());
			return nullptr;
		}
	}

	std::shared_ptr<UgcMod> mod = std::make_shared<UgcMod>();
	mod->type_ = type;
	mod->version_ = static_cast<UgcModVersion>(json["version"].getOrDefault(0));
	mod->isLocal_ = isLocal;

	mod->name_ = *name;
	if (!UTILS::UUID::fromString(*localIdPtr, mod->localId_)) {
		CONSOLE_ERROR("description.json has an invalid 'localId' field for mod at path '%s'.", modPath.string().c_str());
		return nullptr;
	}

	mod->contentPath_ = "$CONTENT_" + mod->localId_.toString();
	mod->fileId_ = json["fileId"].getOrDefault(0);
	mod->thumbnail_.path_ = mod->contentPath_ + "/" + findThumbnailFilenameDirectly(modPath, *name);

	return mod;
}
