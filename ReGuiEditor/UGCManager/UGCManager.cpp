#include "UGCManager.hpp"

#include "UgcMod.hpp"

#include <Filesystem/DirectoryListing.hpp>
#include <Filesystem/DirectoryManager.hpp>

#include <UTILS/Strings.hpp>
#include <UTILS/Uuid.hpp>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/steamtypes.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

void UGCManager::onItemInstalled(ItemInstalled_t* callback) {	
	char filePath[512];
	const bool hasInstallInfo = SteamUGC()->GetItemInstallInfo(callback->m_nPublishedFileId, nullptr, filePath, sizeof(filePath), nullptr);
	if (!hasInstallInfo) {
		CONSOLE_ERROR("Failed to get install info for installed item: %llu", callback->m_nPublishedFileId);
		return;
	}

	(void)this->addMod(filePath, false);
}

void UGCManager::onItemUnsubscribed(RemoteStoragePublishedFileUnsubscribed_t* callback) {
	const auto mod = this->getModByFileId(callback->m_nPublishedFileId);
	if (!mod) {
		CONSOLE_ERROR("UGCManager: Unsubscribed item not found: %llu", callback->m_nPublishedFileId);
		return;
	}

	std::unique_lock lock(this->modsMutex_);
	const auto iterator = std::remove_if(this->mods_.begin(), this->mods_.end(), [&mod](const std::shared_ptr<UgcMod>& entry) -> bool {
		return entry == mod;
	});

	if (iterator == this->mods_.end()) {
		CONSOLE_ERROR("UGCManager: Failed to remove unsubscribed item from mods list: %llu", callback->m_nPublishedFileId);
		return;
	}

	this->mods_.erase(iterator, this->mods_.end());
}

void UGCManager::loadUgcModsFromPath(std::filesystem::path directoryPath, bool isLocal) {
	auto resolvedPath = DirectoryManager::resolveDirectoryStatic(directoryPath.generic_string());
	if (!resolvedPath)
		return;

	auto entries = Filesystem::listDirectory(directoryPath.generic_string(), false, true);
	if (!entries) {
		CONSOLE_ERROR("Failed to list directory: %s", directoryPath.string().c_str());
		return;
	}

	for (const auto& entry : entries.value())
		(void)this->addMod(resolvedPath.value() / entry.name, isLocal);
}

void UGCManager::loadWorkshopMods() {
	uint32 subscriberCount = SteamUGC()->GetNumSubscribedItems();
	if (subscriberCount == 0)
		return;

	std::vector<PublishedFileId_t> subscribedItems(subscriberCount);
	SteamUGC()->GetSubscribedItems(subscribedItems.data(), subscriberCount);

	for (const auto& fileId : subscribedItems) {
		char filePath[512];
		const bool hasInstallInfo = SteamUGC()->GetItemInstallInfo(fileId, nullptr, filePath, sizeof(filePath), nullptr);
		if (!hasInstallInfo) {
			CONSOLE_ERROR("Failed to get install info for subscribed item: %llu", fileId);
			continue;
		}

		const std::filesystem::path installPath(filePath);
		(void)this->addMod(installPath, false);
	}
}

bool UGCManager::initialize() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing UGCManager...");

	// Speed up loading by only loading the Mods folder for now.

	//this->loadUgcModsFromPath("$RGE_USER_DATA/Blueprints", true);
	this->loadUgcModsFromPath("$RGE_USER_DATA/Mods", true);
	//this->loadUgcModsFromPath("$RGE_USER_DATA/Challenges", true);
	//this->loadUgcModsFromPath("$RGE_USER_DATA/Tiles", true);
	//this->loadUgcModsFromPath("$RGE_USER_DATA/Worlds", true);

	this->loadWorkshopMods();

	this->itemInstalledCallback_.Register(this, &UGCManager::onItemInstalled);
	this->itemUnsubscribedCallback_.Register(this, &UGCManager::onItemUnsubscribed);
	this->steamCallbacksRegistered_ = true;

	return true;
}

void UGCManager::cleanup() {
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Cleaning up UGCManager...");

	if (this->steamCallbacksRegistered_) {
		this->itemInstalledCallback_.Unregister();
		this->itemUnsubscribedCallback_.Unregister();
		this->steamCallbacksRegistered_ = false;
	}
}

std::shared_ptr<UgcMod> UGCManager::addMod(const std::filesystem::path& modPath, bool isLocal) {
	auto mod = UgcMod::createFromPath(modPath, isLocal);
	if (!mod) {
		CONSOLE_ERROR("Failed to create UgcMod from path: %s", modPath.string().c_str());
		return nullptr;
	}

	if (!DirectoryManager::getInstance().addDirectory(UTILS::toWideString(mod->getContentPath()), modPath)) {
		CONSOLE_ERROR("UgcMod already registered: %s", mod->getContentPath().c_str());
		return nullptr;
	}

	std::unique_lock lock(this->modsMutex_);
	this->mods_.push_back(mod);

	return mod;
}

const std::vector<std::shared_ptr<UgcMod>>& UGCManager::getMods() {
	std::shared_lock lock(this->modsMutex_);
	return this->mods_;
}

const std::shared_ptr<UgcMod> UGCManager::getModByLocalId(const UTILS::UUID& localId) {
	std::shared_lock lock(this->modsMutex_);
	for (const auto& mod : this->mods_) {
		if (mod->getLocalId() == localId)
			return mod;
	}

	return nullptr;
}

const std::shared_ptr<UgcMod> UGCManager::getModByFileId(uint64_t fileId) {
	std::shared_lock lock(this->modsMutex_);
	for (const auto& mod : this->mods_) {
		if (mod->getFileId() == fileId)
			return mod;
	}

	return nullptr;
}

const std::shared_ptr<UgcMod> UGCManager::getModByContentPath(const std::string& contentPath) {
	std::shared_lock lock(this->modsMutex_);
	for (const auto& mod : this->mods_) {
		if (mod->getContentPath().starts_with(contentPath))
			return mod;
	}

	return nullptr;
}