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

const std::string& UGCModTypeToString(UGCModType type);
UGCModType StringToUGCModType(const std::string& typeStr);

struct UGCMod {
	std::string name;
	std::string localId;
	std::string thumbnailFilepath;
	std::string contentPath;
	std::filesystem::path path;
private:
	ImagePtr thumbnail;
public:
	uint64_t fileId = 0;
	UGCModType type = UGCModType::COUNT;
	
	bool isLocal = false;
	bool hasTriedLoadingThumbnail = false;

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

	[[nodiscard]] static std::shared_ptr<UGCMod> fromPath(const std::filesystem::path& modPath, bool isLocal);
};