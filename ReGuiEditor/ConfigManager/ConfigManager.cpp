#include "ConfigManager.hpp"

#include <Filesystem/FilesystemManager.hpp>

#include <Modules/Bytestream/BytestreamReader.hpp>
#include <Modules/Bytestream/BytestreamWriter.hpp>

#include <cstdint>
#include <string>
#include <variant>

constexpr const char* CONFIG_FILE_PATH = "$RGE_USER_DATA/ReGuiEditor/config.bin";

enum ConfigValueType : uint8_t {
	CONFIG_VALUE_BOOL = 0,
	CONFIG_VALUE_INT = 1,
	CONFIG_VALUE_FLOAT = 2,
	CONFIG_VALUE_STRING = 3
};

ConfigManager::ConfigManager() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing ConfigManager...");

	if (!this->loadConfig())
		CONSOLE_WARNING("Failed to load config. Creating default config...");

	this->createConfig("mygui.canvas.width", 1920);
	this->createConfig("mygui.canvas.height", 1080);

	if (!this->saveConfig())
		CONSOLE_ERROR("Failed to save config.");
}

ConfigManager::~ConfigManager() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Cleaning up ConfigManager...");
}

bool ConfigManager::loadConfig() {
	FilesystemManager& fsManager = FilesystemManager::getInstance();
	if (!fsManager.exists(CONFIG_FILE_PATH))
		return false;

	std::string configContent;
	if (!fsManager.readFile(CONFIG_FILE_PATH, configContent)) {
		CONSOLE_ERROR("Failed to read config file.");
		return false;
	}

	ReGuiEditor::BytestreamReader reader(configContent.data(), configContent.size());
	if (!reader.verifyBytes("RGEC")) { // Identifier check
		CONSOLE_ERROR("Failed to verify config identifier.");
		return false;
	}

	if (!reader.verify<uint8_t>(1)) { // Version check
		CONSOLE_ERROR("Unsupported config version.");
		return false;
	}

	uint16_t numEntries = reader.read<uint16_t>();
	for (uint16_t index = 0; index < numEntries; ++index) {
		std::string key = reader.read<std::string>();
		ConfigValueType valueType = reader.read<ConfigValueType>();

		switch (valueType) {
		case CONFIG_VALUE_BOOL:
			this->data_[key] = reader.read<bool>();
			break;
		case CONFIG_VALUE_INT:
			this->data_[key] = reader.read<int>();
			break;
		case CONFIG_VALUE_FLOAT:
			this->data_[key] = reader.read<float>();
			break;
		case CONFIG_VALUE_STRING:
			this->data_[key] = reader.read<std::string>();
			break;
		default:
			CONSOLE_ERROR("Unknown config value type for entry %d.", index);
			return false;
		}
	}

	return true;
}

bool ConfigManager::saveConfig() {
	if (!this->hasChanges_) {
		CONSOLE_LOG(LEVEL_DEBUG, COLOR_MAGENTA, "No changes detected in config. Skipping save.");
		return true;
	}

	ReGuiEditor::BytestreamWriter writer;
	writer.writeBytes("RGEC", 4);
	writer.write<uint8_t>(1);

	writer.write(static_cast<uint16_t>(this->data_.size()));
	for (const auto& [key, value] : this->data_) {
		writer.write<std::string>(key);

		if (std::holds_alternative<bool>(value)) {
			writer.write(CONFIG_VALUE_BOOL);
			writer.write(std::get<bool>(value));
		} else if (std::holds_alternative<int>(value)) {
			writer.write(CONFIG_VALUE_INT);
			writer.write(std::get<int>(value));
		} else if (std::holds_alternative<float>(value)) {
			writer.write(CONFIG_VALUE_FLOAT);
			writer.write(std::get<float>(value));
		} else if (std::holds_alternative<std::string>(value)) {
			writer.write(CONFIG_VALUE_STRING);
			writer.write<std::string>(std::get<std::string>(value));
		} else {
			CONSOLE_ERROR("Unknown config value type for key: %s", key.c_str());
			return false;
		}
	}

	if (!FilesystemManager::getInstance().writeFile(CONFIG_FILE_PATH, writer)) {
		CONSOLE_ERROR("Failed to write config file.");
		return false;
	}

	this->hasChanges_ = false;
	return true;
}