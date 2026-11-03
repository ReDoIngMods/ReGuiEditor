#include "UgcMod.hpp"

#include <Modules/Misc/Expected.hpp>

#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <system_error>

#include <Modules/JSON/JSONObject.hpp>
#include <Modules/JSON/JSONParser.hpp>
#include <Modules/UniqueIdentifiers/Uuid.hpp>

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

ReGuiEditor::Expected<std::shared_ptr<ReGuiEditor::UgcMod>> ReGuiEditor::UgcMod::createFromPath(std::filesystem::path modPath, bool isLocal) {
	modPath.make_preferred();

	std::string descriptionData;
	if (!readFileDirectly(modPath / "description.json", descriptionData))
		return ReGuiEditor::ExpectedError("Failed to read description.json");

	ReGuiEditor::JSONValue json;
	if (!ReGuiEditor::JSONParser::parseJson(descriptionData, json))
		return ReGuiEditor::ExpectedError("Failed to parse description.json");

	const std::string* name = json["name"].tryGet<std::string>();
	if (!name)
		return ReGuiEditor::ExpectedError("description.json is missing the 'name' field");
	
	const std::string* localIdPtr = json["localId"].tryGet<std::string>();
	if (!localIdPtr) 
		return ReGuiEditor::ExpectedError("description.json is missing the 'localId' field");

	ReGuiEditor::UgcModType type = ReGuiEditor::UgcModType::Blueprint;
	
	const std::string* typeStr = json["type"].tryGet<std::string>();
	if (typeStr) {
		type = ReGuiEditor::stringToUgcModType(typeStr->c_str());
		if (type == ReGuiEditor::UgcModType::COUNT)
			return ReGuiEditor::ExpectedError("description.json has an invalid 'type' field");
	}

	std::shared_ptr<ReGuiEditor::UgcMod> mod = std::make_shared<ReGuiEditor::UgcMod>();
	mod->type_ = type;
	mod->version_ = static_cast<ReGuiEditor::UgcModVersion>(json["version"].getOrDefault(0));
	mod->isLocal_ = isLocal;
	
	mod->name_ = *name;
	if (!ReGuiEditor::Uuid::fromString(*localIdPtr, mod->localId_))
		return ReGuiEditor::ExpectedError("description.json has an invalid 'localId' field");

	mod->contentPath_ = "$CONTENT_" + mod->localId_.toString();
	mod->fileId_ = json["fileId"].getOrDefault(0);
	mod->thumbnail_.path_ = mod->contentPath_ + "/" + findThumbnailFilenameDirectly(modPath, *name);

	return mod;
}