#include "UgcService.hpp"

#include "UgcMod.hpp"

#include <Filesystem/DirectoryManager.hpp>
#include <Filesystem/FilesystemManager.hpp>

#include <Modules/UniqueIdentifiers/Uuid.hpp>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/steamtypes.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

void ReGuiEditor::UgcService::onItemInstalled(ItemInstalled_t* callback) {
	CONSOLE_LOG(LEVEL_SERVICE, COLOR_CYAN, "UgcService: Item installed: %llu", callback->m_nPublishedFileId);
}

void ReGuiEditor::UgcService::onItemUnsubscribed(RemoteStoragePublishedFileUnsubscribed_t* callback) {
	CONSOLE_LOG(LEVEL_SERVICE, COLOR_CYAN, "UgcService: Item unsubscribed: %llu", callback->m_nPublishedFileId);
}

void ReGuiEditor::UgcService::loadUgcModsFromPath(std::filesystem::path directoryPath, bool isLocal) {
	std::filesystem::path resolvedPath;
	if (!ReGuiEditor::DirectoryManager::getInstance().resolveDirectory(directoryPath.generic_string(), resolvedPath))
		return;

	ReGuiEditor::FilesystemManager& filesystemManager = ReGuiEditor::FilesystemManager::getInstance();
	auto entries = filesystemManager.listDirectory(directoryPath.generic_string(), false, true);
	if (!entries.has_value()) {
		CONSOLE_ERROR("Failed to list directory: %s", directoryPath.string().c_str());
		return;
	}

	for (const auto& entry : entries.value())
		(void)this->addMod(resolvedPath / entry.name, isLocal);
}

void ReGuiEditor::UgcService::loadWorkshopMods() {
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

bool ReGuiEditor::UgcService::initialize() {
	CONSOLE_LOG(LEVEL_SERVICE, COLOR_CYAN, "Initializing UgcService...");

	this->loadUgcModsFromPath("$RGE_USER_DATA/Blueprints", true);
	this->loadUgcModsFromPath("$RGE_USER_DATA/Mods",       true);
	this->loadUgcModsFromPath("$RGE_USER_DATA/Challenges", true);
	this->loadUgcModsFromPath("$RGE_USER_DATA/Tiles",      true);
	this->loadUgcModsFromPath("$RGE_USER_DATA/Worlds",     true);

	this->loadWorkshopMods();

	this->itemInstalledCallback_.Register(this, &ReGuiEditor::UgcService::onItemInstalled);
	this->itemUnsubscribedCallback_.Register(this, &ReGuiEditor::UgcService::onItemUnsubscribed);
	this->steamCallbacksRegistered_ = true;

	return true;
}

void ReGuiEditor::UgcService::cleanup() {
	CONSOLE_LOG(LEVEL_SERVICE, COLOR_CYAN, "Cleaning up UgcService...");

	if (this->steamCallbacksRegistered_) {
		this->itemInstalledCallback_.Unregister();
		this->itemUnsubscribedCallback_.Unregister();
		this->steamCallbacksRegistered_ = false;
	}
}

std::shared_ptr<ReGuiEditor::UgcMod> ReGuiEditor::UgcService::addMod(const std::filesystem::path& modPath, bool isLocal) {
	auto modResult = ReGuiEditor::UgcMod::createFromPath(modPath, isLocal);
	if (!modResult.has_value()) {
		CONSOLE_ERROR("Failed to create UgcMod from path: %s, %s", modPath.string().c_str(), modResult.get_error_message().data());
		return nullptr;
	}

	if (!ReGuiEditor::DirectoryManager::getInstance().addDirectory(modResult.value()->getContentPath(), modPath)) {
		CONSOLE_ERROR("UgcMod already registered: %s", modResult.value()->getContentPath().c_str());
		return nullptr;
	}

	std::unique_lock lock(this->modsMutex_);
	this->mods_.push_back(modResult.value());

	return modResult.value();
}

const std::vector<std::shared_ptr<ReGuiEditor::UgcMod>>& ReGuiEditor::UgcService::getMods() {
	std::shared_lock lock(this->modsMutex_);
	return this->mods_;
}

const std::shared_ptr<ReGuiEditor::UgcMod> ReGuiEditor::UgcService::getModByLocalId(const ReGuiEditor::Uuid& localId) {
	std::shared_lock lock(this->modsMutex_);
	for (const auto& mod : this->mods_) {
		if (mod->getLocalId() == localId)
			return mod;
	}

	return nullptr;
}

const std::shared_ptr<ReGuiEditor::UgcMod> ReGuiEditor::UgcService::getModByFileId(uint64_t fileId) {
	std::shared_lock lock(this->modsMutex_);
	for (const auto& mod : this->mods_) {
		if (mod->getFileId() == fileId)
			return mod;
	}

	return nullptr;
}

const std::shared_ptr<ReGuiEditor::UgcMod> ReGuiEditor::UgcService::getModByContentPath(const std::string& contentPath) {
	std::shared_lock lock(this->modsMutex_);
	for (const auto& mod : this->mods_) {
		if (mod->getContentPath().starts_with(contentPath))
			return mod;
	}

	return nullptr;
}