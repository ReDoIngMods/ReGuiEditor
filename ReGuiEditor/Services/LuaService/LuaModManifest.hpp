#pragma once
#include <Modules/UniqueIdentifiers/Uuid.hpp>

#include <string>

namespace ReGuiEditor {
	struct LuaModManifest {
		static constexpr const char* FILE_PATH = "$CONTENT_DATA/reguieditor.json";
		static constexpr int SUPPORTED_VERSION = 1;

		int version = 0;
		std::string scriptFilename;
		std::string className;

		[[nodiscard]] static bool exists(const ReGuiEditor::Uuid& modLocalId);
		[[nodiscard]] static bool load(const ReGuiEditor::Uuid& modLocalId, LuaModManifest& output, std::string& error);
	};
}
