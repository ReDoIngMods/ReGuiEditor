#include "LuaModManifest.hpp"

#include <Filesystem/DirectoryManager.hpp>
#include <Filesystem/FilesystemManager.hpp>

#include <Modules/JSON/JSONObject.hpp>
#include <Modules/Misc/Strings.hpp>
#include <Modules/UniqueIdentifiers/Uuid.hpp>

#include <string>
#include <utility>

static bool readString(const ReGuiEditor::JSONValue& parent, const char* key, std::string& output, std::string& error) {
	const std::string* value = parent.at(key).tryGet<std::string>();
	if (!value || value->empty()) {
		error = ReGuiEditor::formatString("'%s' must be a non-empty string", key);
		return false;
	}

	output = *value;
	return true;
}

bool ReGuiEditor::LuaModManifest::exists(const ReGuiEditor::Uuid& modLocalId) {
	std::string path = FILE_PATH;
	if (!DirectoryManager::getInstance().resolveContentData(path, modLocalId))
		return false;

	return FilesystemManager::getInstance().exists(path);
}

bool ReGuiEditor::LuaModManifest::load(const ReGuiEditor::Uuid& modLocalId, LuaModManifest& output, std::string& error) {
	std::string path = FILE_PATH;
	if (!DirectoryManager::getInstance().resolveContentData(path, modLocalId)) {
		error = ReGuiEditor::formatString("cannot resolve path '%s'", FILE_PATH);
		return false;
	}

	JSONValue document;
	if (!FilesystemManager::getInstance().readFile(path, document)) {
		error = ReGuiEditor::formatString("cannot read or parse '%s'", path.c_str());
		return false;
	}

	if (!document.is<JSONObject>()) {
		error = "the root must be an object";
		return false;
	}

	const double* version = document.at("version").tryGet<double>();
	if (!version) {
		error = "'version' must be a number";
		return false;
	}

	if (static_cast<int>(*version) != SUPPORTED_VERSION) {
		error = ReGuiEditor::formatString("unsupported version %d (supported: %d)", static_cast<int>(*version), SUPPORTED_VERSION);
		return false;
	}

	const JSONValue& script = document.at("script");
	if (!script.is<JSONObject>()) {
		error = "'script' must be an object";
		return false;
	}

	LuaModManifest manifest;
	manifest.version = static_cast<int>(*version);

	std::string scriptError;
	if (!readString(script, "filename", manifest.scriptFilename, scriptError) || !readString(script, "classname", manifest.className, scriptError)) {
		error = "script." + scriptError;
		return false;
	}

	output = std::move(manifest);
	return true;
}
