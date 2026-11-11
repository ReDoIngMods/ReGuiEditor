#include "UGCMod.hpp"

#include <JSON/JSON.hpp>

#include <UTILS/Uuid.hpp>

#include <cstdint>
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

	JSONValue document;
	JSONParserResult result = JSONValue::parse(descriptionData, document);
	if (!result) {
		CONSOLE_ERROR("Failed to parse description.json for mod at path '%s': %s", modPath.string().c_str(), result.createErrorMessage().c_str());
		return nullptr;
	}

	if (!document.isObject()) {
		CONSOLE_ERROR("description.json is not a valid JSON object for mod at path '%s'.", modPath.string().c_str());
		return nullptr;
	}

	const JSONObject& jsonObject = document.asObject();

	const auto nameIterator = jsonObject.find("name");
	if (nameIterator == jsonObject.end() || !nameIterator->second.isString()) {
		CONSOLE_ERROR("description.json is missing the 'name' field for mod at path '%s'.", modPath.string().c_str());
		return nullptr;
	}

	const auto localIdIterator = jsonObject.find("localId");
	if (localIdIterator == jsonObject.end() || !localIdIterator->second.isString()) {
		CONSOLE_ERROR("description.json is missing the 'localId' field for mod at path '%s'.", modPath.string().c_str());
		return nullptr;
	}

	const std::string& name = nameIterator->second.asString();

	auto localIdOpt = UTILS::UUID::fromString(localIdIterator->second.asString());
	if (!localIdOpt) {
		CONSOLE_ERROR("description.json has an invalid 'localId' field for mod at path '%s'.", modPath.string().c_str());
		return nullptr;
	}

	UgcModType type = UgcModType::Blueprint;

	const auto typeIterator = jsonObject.find("type");
	if (typeIterator != jsonObject.end() && typeIterator->second.isString()) {
		type = stringToUgcModType(typeIterator->second.asString().c_str());

		if (type == UgcModType::COUNT) {
			CONSOLE_ERROR("description.json has an invalid 'type' field for mod at path '%s'.", modPath.string().c_str());
			return nullptr;
		}
	}

	std::shared_ptr<UgcMod> mod = std::make_shared<UgcMod>();
	mod->type_ = type;
	mod->version_ = static_cast<UgcModVersion>(jsonObject.contains("version") ? jsonObject.at("version").getInteger().value_or(0) : 0);
	mod->isLocal_ = isLocal;
	mod->name_ = name;
	mod->localId_ = *localIdOpt;
	mod->contentPath_ = "$CONTENT_" + mod->localId_.toAStr();
	mod->fileId_ = jsonObject.contains("fileId") ? static_cast<uint64_t>(jsonObject.at("fileId").getDouble().value_or(0.0)) : 0;
	mod->thumbnail_.path_ = mod->contentPath_ + "/" + findThumbnailFilenameDirectly(modPath, name);

	return mod;
}
