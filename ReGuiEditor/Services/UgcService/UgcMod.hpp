#pragma once
#include <GUI/ImageManager.hpp>

#include <Modules/Misc/Expected.hpp>
#include <Modules/UniqueIdentifiers/Uuid.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

namespace ReGuiEditor {
	typedef enum class UgcModType : uint16_t {
		Blueprint,
		BlocksAndParts,
		Tile,
		World,
		TerrainAssets,
		ChallengePack,
		ChallengeLevel,
		CustomGame,

		COUNT
	} UgcModType;

	inline const char* ugcModTypeToString(ReGuiEditor::UgcModType type) {
		switch (type) {
		case ReGuiEditor::UgcModType::Blueprint:      return "Blueprint";
		case ReGuiEditor::UgcModType::BlocksAndParts: return "Blocks and Parts";
		case ReGuiEditor::UgcModType::Tile:           return "Tile";
		case ReGuiEditor::UgcModType::World:          return "World";
		case ReGuiEditor::UgcModType::TerrainAssets:  return "Terrain Assets";
		case ReGuiEditor::UgcModType::ChallengePack:  return "Challenge Pack";
		case ReGuiEditor::UgcModType::ChallengeLevel: return "Challenge Level";
		case ReGuiEditor::UgcModType::CustomGame:     return "Custom Game";
		default: return "Unknown";
		}
	}

	inline ReGuiEditor::UgcModType stringToUgcModType(const char* typeStr) {
		if (strncmp(typeStr, "Blueprint",         9) == 0) return ReGuiEditor::UgcModType::Blueprint;
		if (strncmp(typeStr, "Blocks and Parts", 16) == 0) return ReGuiEditor::UgcModType::BlocksAndParts;
		if (strncmp(typeStr, "Tile",              4) == 0) return ReGuiEditor::UgcModType::Tile;
		if (strncmp(typeStr, "World",             5) == 0) return ReGuiEditor::UgcModType::World;
		if (strncmp(typeStr, "Terrain Assets",   14) == 0) return ReGuiEditor::UgcModType::TerrainAssets;
		if (strncmp(typeStr, "Challenge Pack",   14) == 0) return ReGuiEditor::UgcModType::ChallengePack;
		if (strncmp(typeStr, "Challenge Level",  15) == 0) return ReGuiEditor::UgcModType::ChallengeLevel;
		if (strncmp(typeStr, "Custom Game",      11) == 0) return ReGuiEditor::UgcModType::CustomGame;
		return ReGuiEditor::UgcModType::COUNT;
	}

	typedef enum class UgcModVersion : uint16_t {
		PreSurvival     = 0,
		Survival        = 1,
		DrillingThunder = 2,
		COUNT
	} UgcModVersion;

	inline const char* UgcModVersionToString(ReGuiEditor::UgcModVersion version) {
		switch (version) {
		case ReGuiEditor::UgcModVersion::PreSurvival:     return "Pre-Survival";
		case ReGuiEditor::UgcModVersion::Survival:        return "Survival";
		case ReGuiEditor::UgcModVersion::DrillingThunder: return "Drilling Thunder";
		default: return "Unknown";
		}
	}

	class UgcMod {
		ReGuiEditor::UgcModType type_ = ReGuiEditor::UgcModType::COUNT;
		ReGuiEditor::UgcModVersion version_ = ReGuiEditor::UgcModVersion::COUNT;
		bool isLocal_ = false;

		std::string name_;
		std::string contentPath_;

		ReGuiEditor::Uuid localId_;
		uint64_t fileId_ = 0;

		struct {
			std::string path_;
			ImagePtr image_ = nullptr;
			bool hasAttemptedToLoad_ = false;
		} thumbnail_;
	public:
		[[nodiscard]] ReGuiEditor::UgcModType getType() const { return this->type_; }
		[[nodiscard]] ReGuiEditor::UgcModVersion getVersion() const { return this->version_; }
		[[nodiscard]] bool isLocal() const { return this->isLocal_; }

		[[nodiscard]] const std::string& getName() const { return this->name_; }
		[[nodiscard]] const std::string& getContentPath() const { return this->contentPath_; }

		[[nodiscard]] const ReGuiEditor::Uuid& getLocalId() const { return this->localId_; }
		[[nodiscard]] uint64_t getFileId() const { return this->fileId_; }

		[[nodiscard]] ImagePtr getThumbnail() {
			if (this->thumbnail_.image_ || this->thumbnail_.hasAttemptedToLoad_)
				return this->thumbnail_.image_;

			ImageManager* imageManager = ImageManager::getInstancePtr();
			if (!imageManager)
				return nullptr;

			this->thumbnail_.image_ = imageManager->findOrLoadImage(this->thumbnail_.path_);
			this->thumbnail_.hasAttemptedToLoad_ = true;
			return this->thumbnail_.image_;
		}
		[[nodiscard]] const char* getVersionName() const {
			switch (this->version_) {
			case ReGuiEditor::UgcModVersion::PreSurvival:     return "Pre-Survival";
			case ReGuiEditor::UgcModVersion::Survival:        return "Survival";
			case ReGuiEditor::UgcModVersion::DrillingThunder: return "Drilling Thunder";
			default: return "Unknown Version";
			}
		}

		[[nodiscard]] bool isOutdated() const { return this->version_ <= ReGuiEditor::UgcModVersion::Survival; }

		static ReGuiEditor::Expected<std::shared_ptr<ReGuiEditor::UgcMod>> createFromPath(std::filesystem::path modPath);
	};
}