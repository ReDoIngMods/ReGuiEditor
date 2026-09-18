#include "ConfigManager.hpp"

#include <Filesystem/Filesystem.hpp>

#include <UTILS/ByteStreamReader.hpp>
#include <UTILS/ByteStreamWriter.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <variant>

enum ConfigValueType : uint8_t {
	CONFIG_VALUE_BOOL = 0,
	CONFIG_VALUE_INT = 1,
	CONFIG_VALUE_FLOAT = 2,
	CONFIG_VALUE_STRING = 3
};

ConfigManager::ConfigManager() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing ConfigManager...");

	if (!this->loadConfig())
		CONSOLE_LOG(LEVEL_WARNING, COLOR_YELLOW, "Failed to load config. Creating default config...");

	this->createConfig("mygui.canvas.width", 1920);
	this->createConfig("mygui.canvas.height", 1080);

	if (!this->saveConfig())
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to save config.");
}

ConfigManager::~ConfigManager() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Cleaning up ConfigManager...");
}

bool ConfigManager::loadConfig() {
	if (!FS::fileExists("config.bin"))
		return false;

	std::string configContent;
	if (!FS::readFile("config.bin", configContent)) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to read config file.");
		return false;
	}

	UTILS::ByteStreamReader reader(configContent.data(), configContent.size());

	{ // Identifier check
		char identifier[4];
		if (!reader.readBytes(identifier, sizeof(identifier))) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to read config identifier.");
			return false;
		}

		if (strncmp(identifier, "RGEC", 4) != 0) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Invalid config identifier.");
			return false;
		}
	}

	{ // Version check
		uint8_t version = reader.readByte();
		if (version != 1) {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Unsupported config version: %d", version);
			return false;
		}
	}

	uint16_t numEntries = reader.read<uint16_t>();
	for (uint16_t index = 0; index < numEntries; ++index) {
		std::string key = reader.readString();
		ConfigValueType valueType = static_cast<ConfigValueType>(reader.readByte());

		switch (valueType) {
		case CONFIG_VALUE_BOOL: {
			this->data_[key] = reader.read<bool>();
			break;
		}
		case CONFIG_VALUE_INT: {
			this->data_[key] = reader.read<int>();
			break;
		}
		case CONFIG_VALUE_FLOAT: {
			this->data_[key] = reader.read<float>();
			break;
		}
		case CONFIG_VALUE_STRING: {
			this->data_[key] = reader.readString();
			break;
		}
		default:
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Unknown config value type for entry %d.", index);
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

	UTILS::ByteStreamWriter writer;
	writer.writeBytes("RGEC", 4);
	writer.writeByte(1);

	writer.write(static_cast<uint16_t>(this->data_.size()));
	for (const auto& [key, value] : this->data_) {
		writer.writeString(key);

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
			writer.writeString(std::get<std::string>(value));
		} else {
			CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Unknown config value type for key: %s", key.c_str());
			return false;
		}
	}

	if (!FS::writeFile("config.bin", writer.toString())) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to write config file.");
		return false;
	}

	this->hasChanges_ = false;
	return true;
}