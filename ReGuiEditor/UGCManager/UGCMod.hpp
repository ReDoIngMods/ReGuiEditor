#pragma once
#include <GUI/ImageManager.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

enum class UGCModType : uint8_t {
	Blueprint,
	BlocksAndParts,
	Tile,
	World,
	TerrainAssets,
	ChallengePack,
	ChallengeLevel,
	CustomGame,

	COUNT
};

const char* UGCModTypeToString(UGCModType type);
UGCModType StringToUGCModType(const char* typeStr);

struct UGCMod {
	std::string name;
	std::string localId;
	std::string thumbnailFilepath;
	std::string contentPath;
	std::filesystem::path path;

	uint64_t fileId = 0;
	int version = 0;
	UGCModType type = UGCModType::COUNT;
	
	bool isLocal = false;
	bool hasTriedLoadingThumbnail = false;
private:
	ImagePtr thumbnail = nullptr;
public:
	[[nodiscard]] ImagePtr getThumbnail() {
		if (this->thumbnail || this->hasTriedLoadingThumbnail)
			return this->thumbnail;

		ImageManager* imageManager = ImageManager::getInstancePtr();
		if (!imageManager)
			return nullptr;
	
		this->thumbnail = imageManager->findOrLoadImage(this->thumbnailFilepath);
		this->hasTriedLoadingThumbnail = true;
		return this->thumbnail;
	}

	[[nodiscard]] bool isOutdated() const {
		return this->version <= 1; // Pre Drilling Thunder and below = outdated.
	}

	[[nodiscard]] const char* getVersionName() const {
		switch (this->version) {
		case 0: return "Pre-Survival";
		case 1: return "Chapter 1";
		case 2: return "Chapter 2 - Drilling Thunder";
		default: return "Unknown Version";
		}
	}

	[[nodiscard]] static std::shared_ptr<UGCMod> fromPath(const std::filesystem::path& modPath, bool isLocal);
};