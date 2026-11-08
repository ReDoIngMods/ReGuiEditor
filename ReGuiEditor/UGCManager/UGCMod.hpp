#pragma once
#include <GUI/ImageManager.hpp>

#include <UTILS/Uuid.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

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

inline const char* ugcModTypeToString(UgcModType type) {
	switch (type) {
	case UgcModType::Blueprint:      return "Blueprint";
	case UgcModType::BlocksAndParts: return "Blocks and Parts";
	case UgcModType::Tile:           return "Tile";
	case UgcModType::World:          return "World";
	case UgcModType::TerrainAssets:  return "Terrain Assets";
	case UgcModType::ChallengePack:  return "Challenge Pack";
	case UgcModType::ChallengeLevel: return "Challenge Level";
	case UgcModType::CustomGame:     return "Custom Game";
	default: return "Unknown";
	}
}

inline UgcModType stringToUgcModType(const char* typeStr) {
	if (strncmp(typeStr, "Blueprint", 9) == 0) return UgcModType::Blueprint;
	if (strncmp(typeStr, "Blocks and Parts", 16) == 0) return UgcModType::BlocksAndParts;
	if (strncmp(typeStr, "Tile", 4) == 0) return UgcModType::Tile;
	if (strncmp(typeStr, "World", 5) == 0) return UgcModType::World;
	if (strncmp(typeStr, "Terrain Assets", 14) == 0) return UgcModType::TerrainAssets;
	if (strncmp(typeStr, "Challenge Pack", 14) == 0) return UgcModType::ChallengePack;
	if (strncmp(typeStr, "Challenge Level", 15) == 0) return UgcModType::ChallengeLevel;
	if (strncmp(typeStr, "Custom Game", 11) == 0) return UgcModType::CustomGame;
	return UgcModType::COUNT;
}

typedef enum class UgcModVersion : uint16_t {
	PreSurvival = 0,
	Survival = 1,
	DrillingThunder = 2,
	COUNT
} UgcModVersion;

inline const char* UgcModVersionToString(UgcModVersion version) {
	switch (version) {
	case UgcModVersion::PreSurvival:     return "Pre-Survival";
	case UgcModVersion::Survival:        return "Survival";
	case UgcModVersion::DrillingThunder: return "Drilling Thunder";
	default: return "Unknown";
	}
}

class UgcMod {
	UgcModType type_ = UgcModType::COUNT;
	UgcModVersion version_ = UgcModVersion::COUNT;
	bool isLocal_ = false;

	std::string name_;
	std::string contentPath_;

	UTILS::UUID localId_;
	uint64_t fileId_ = 0;

	struct {
		std::string path_;
		ImagePtr image_ = nullptr;
		bool hasAttemptedToLoad_ = false;
	} thumbnail_;
public:
	[[nodiscard]] UgcModType getType() const { return this->type_; }
	[[nodiscard]] UgcModVersion getVersion() const { return this->version_; }
	[[nodiscard]] bool isLocal() const { return this->isLocal_; }

	[[nodiscard]] const std::string& getName() const { return this->name_; }
	[[nodiscard]] const std::string& getContentPath() const { return this->contentPath_; }

	[[nodiscard]] const UTILS::UUID& getLocalId() const { return this->localId_; }
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
		case UgcModVersion::PreSurvival:     return "Pre-Survival";
		case UgcModVersion::Survival:        return "Survival";
		case UgcModVersion::DrillingThunder: return "Drilling Thunder";
		default: return "Unknown Version";
		}
	}

	[[nodiscard]] bool isOutdated() const { return this->version_ <= UgcModVersion::Survival; }

	static std::shared_ptr<UgcMod> createFromPath(std::filesystem::path modPath, bool isLocal);
};