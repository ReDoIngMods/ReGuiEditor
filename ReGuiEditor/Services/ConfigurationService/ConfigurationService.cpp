#include "ConfigurationService.hpp"

#include <Modules/Bytestream/BytestreamReader.hpp>
#include <Modules/Bytestream/BytestreamWriter.hpp>

#include <Filesystem/FilesystemManager.hpp>

#include <cstdint>
#include <string>
#include <string.h>
#include <variant>

constexpr const char* CONFIG_FILE_PATH = "$RGE_USER_DATA/ReGuiData/Config/config.bin";

struct ConfigurationFileHeader {
	char magic[4] = { 'R', 'G', 'E', 'C' };
	uint32_t version = 2;

	bool isValid() const {
		return memcmp(this->magic, "RGEC", 4) == 0 && this->version == 2;
	}
};

enum class ConfigurationPropertyType : uint8_t {
	Boolean = 0,
	Integer = 1,
	Float = 2,
	String = 3
};

void ReGuiEditor::ConfigurationService::saveConfiguration() {
	ReGuiEditor::BytestreamWriter writer;
	writer.write<ConfigurationFileHeader>();

    for (const auto& property : this->properties_) {
        writer.write<std::string>(property.key);
        
		if (std::holds_alternative<bool>(property.value)) {
			writer.write(ConfigurationPropertyType::Boolean);
			writer.write(std::get<bool>(property.value));
        } else if (std::holds_alternative<int>(property.value)) {
            writer.write(ConfigurationPropertyType::Integer);
            writer.write(std::get<int>(property.value));
        } else if (std::holds_alternative<float>(property.value)) {
            writer.write(ConfigurationPropertyType::Float);
            writer.write(std::get<float>(property.value));
        } else if (std::holds_alternative<std::string>(property.value)) {
            writer.write(ConfigurationPropertyType::String);
            writer.write(std::get<std::string>(property.value));
        }
	}

	ReGuiEditor::FilesystemManager& filesystemManager = ReGuiEditor::FilesystemManager::getInstance();
	if (!filesystemManager.writeFile(CONFIG_FILE_PATH, writer))
		CONSOLE_ERROR("Failed to save configuration to file: %s", CONFIG_FILE_PATH);
}

bool ReGuiEditor::ConfigurationService::initialize() {
    ReGuiEditor::FilesystemManager& filesystemManager = ReGuiEditor::FilesystemManager::getInstance();

    ReGuiEditor::BytestreamReader reader;
    if (!filesystemManager.readFile(CONFIG_FILE_PATH, reader))
        return true;

	if (ConfigurationFileHeader header = reader.read<ConfigurationFileHeader>(); !header.isValid()) {
		CONSOLE_ERROR("Invalid configuration file header in file: %s", CONFIG_FILE_PATH);
		return false;
	}

	while (!reader.isEOF()) {
		std::string key = reader.read<std::string>();
		ConfigurationValue value;

		switch (reader.read<ConfigurationPropertyType>()) {
		case ConfigurationPropertyType::Boolean: value = reader.read<bool       >(); break;
		case ConfigurationPropertyType::Integer: value = reader.read<int        >(); break;
		case ConfigurationPropertyType::Float:   value = reader.read<float      >(); break;
		case ConfigurationPropertyType::String:  value = reader.read<std::string>(); break;
		default:
			CONSOLE_WARNING("Unknown property type in configuration file for key: %s", key.c_str());
			continue;
		}

		this->properties_.emplace_back(key, value);
	}

	return true;
}

void ReGuiEditor::ConfigurationService::tick() {
    if (!this->isDirty_)
        return;

    this->saveConfiguration();
    this->isDirty_ = false;
}
