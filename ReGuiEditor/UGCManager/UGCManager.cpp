#include "UGCManager.hpp"

#include "UGCMod.hpp"

#include <Application.hpp>

#include <Filesystem/DirectoryManager.hpp>

#include <Modules/Misc/Strings.hpp>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/isteamutils.h>
#include <steam/steamtypes.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

inline static bool getItemInstallPath(PublishedFileId_t fileId, std::filesystem::path& itemPath) {
	char buffer[1024];
	if (!SteamUGC()->GetItemInstallInfo(fileId, nullptr, buffer, sizeof(buffer), nullptr))
		return false;

	itemPath = buffer;
	return true;
}

inline static bool isOwnApp(AppId_t appId) {
	return appId == SteamUtils()->GetAppID();
}

bool UGCManager::registerMod(const std::filesystem::path& modPath, bool isLocal) {
	const std::shared_ptr<UGCMod> mod = UGCMod::fromPath(modPath, isLocal);
	if (!mod) {
		CONSOLE_ERROR("Failed to create UGCMod from path: %s", modPath.string().c_str());
		return false;
	}

	if (!DirectoryManager::getInstance().addDirectory(mod->contentPath, modPath)) {
		CONSOLE_WARNING("Mod: %s was already loaded with localId: %s", modPath.string().c_str(), mod->localId.c_str());
		return false;
	}

	Application::getInstance().splashscreen.setStatus(ReGuiEditor::formatString(L"Loading mod: %hs (%hs)", mod->localId.c_str(), modPath.string().c_str()));

	this->mods_.push_back(mod);
	if (mod->fileId != 0)
		this->fileIdToLocalId_[mod->fileId] = mod->localId;

	if (isLocal)
		this->localMods_.push_back(mod);
	else
		this->workshopMods_.push_back(mod);

	return true;
}

void UGCManager::registerWorkshopItem(PublishedFileId_t fileId) {
	if (this->fileIdToLocalId_.contains(fileId))
		return;

	std::filesystem::path itemPath;
	if (!getItemInstallPath(fileId, itemPath)) {
		CONSOLE_ERROR("Failed to get the installation path for Steam Workshop item: %llu", static_cast<unsigned long long>(fileId));
		return;
	}

	this->registerMod(itemPath, false);
}

bool UGCManager::loadLocalMods() {
	std::filesystem::path modsRoot;
	if (!DirectoryManager::getInstance().resolveDirectory("$RGE_USER_DATA/Mods", modsRoot)) {
		CONSOLE_ERROR("Failed to resolve the local mods folder.");
		return false;
	}

	std::error_code error;
	if (!std::filesystem::exists(modsRoot, error)) {
		CONSOLE_ERROR("Local mods folder does not exist: %s", modsRoot.string().c_str());
		return true;
	}

	for (std::filesystem::directory_iterator iterator(modsRoot, error), end; !error && iterator != end; iterator.increment(error)) {
		if (iterator->is_directory(error))
			this->registerMod(iterator->path(), true);
	}

	if (error) {
		CONSOLE_ERROR("Failed to list the local mods folder: %s", error.message().c_str());
		return false;
	}

	return true;
}

void UGCManager::loadUGCMods() {
	std::vector<PublishedFileId_t> subscribedItems(SteamUGC()->GetNumSubscribedItems());
	subscribedItems.resize(SteamUGC()->GetSubscribedItems(subscribedItems.data(), static_cast<uint32_t>(subscribedItems.size())));

	for (const PublishedFileId_t fileId : subscribedItems)
		this->registerWorkshopItem(fileId);
}

void UGCManager::onItemInstalled(ItemInstalled_t* pCallback) {
	if (pCallback == nullptr || !isOwnApp(pCallback->m_unAppID))
		return;

	CONSOLE_LOG(LEVEL_DEBUG, COLOR_CYAN, "Steam Workshop item installed: %llu", static_cast<unsigned long long>(pCallback->m_nPublishedFileId));
	this->registerWorkshopItem(pCallback->m_nPublishedFileId);
}

void UGCManager::onItemUnsubscribed(RemoteStoragePublishedFileUnsubscribed_t* pCallback) {
	if (pCallback == nullptr || !isOwnApp(pCallback->m_nAppID))
		return;

	const unsigned long long fileId = pCallback->m_nPublishedFileId;
	CONSOLE_LOG(LEVEL_DEBUG, COLOR_CYAN, "Steam Workshop item unsubscribed: %llu", fileId);

	const auto fileIdIterator = this->fileIdToLocalId_.find(pCallback->m_nPublishedFileId);
	if (fileIdIterator == this->fileIdToLocalId_.end()) {
		CONSOLE_WARNING("Steam Workshop item: %llu is not in the local ID map.", fileId);
		return;
	}

	const std::string& localId = fileIdIterator->second;
	const auto iterator = std::find_if(this->mods_.begin(), this->mods_.end(), [&localId](const std::shared_ptr<UGCMod>& mod) {
		return mod->localId == localId;
	});

	if (iterator == this->mods_.end()) {
		CONSOLE_WARNING("Steam Workshop item: %llu with localId: %s is not in the mods list.", fileId, localId.c_str());
		return;
	}

	const std::shared_ptr<UGCMod> mod = *iterator;
	if (!DirectoryManager::getInstance().removeDirectory(mod->contentPath))
		CONSOLE_WARNING("Steam Workshop item: %llu with localId: %s is not in the directories map.", fileId, localId.c_str());

	std::erase(this->workshopMods_, mod);
	this->mods_.erase(iterator);
	this->fileIdToLocalId_.erase(fileIdIterator);
}

bool UGCManager::initialize() {
	if (this->callbacksRegistered_)
		return true;

	if (!this->loadLocalMods()) {
		CONSOLE_ERROR("Failed to load local mods.");
		return false;
	}

	this->loadUGCMods();

	this->itemInstalledCallback_.Register(this, &UGCManager::onItemInstalled);
	this->itemUnsubscribedCallback_.Register(this, &UGCManager::onItemUnsubscribed);
	this->callbacksRegistered_ = true;

	return true;
}

void UGCManager::cleanup() {
	if (this->callbacksRegistered_) {
		this->itemInstalledCallback_.Unregister();
		this->itemUnsubscribedCallback_.Unregister();
		this->callbacksRegistered_ = false;
	}

	DirectoryManager& directoryManager = DirectoryManager::getInstance();
	for (const auto& mod : this->mods_)
		directoryManager.removeDirectory(mod->contentPath);

	this->fileIdToLocalId_.clear();
	this->workshopMods_.clear();
	this->localMods_.clear();
	this->mods_.clear();
}

[[nodiscard]] std::shared_ptr<UGCMod> UGCManager::getModByLocalId(const std::string& localId) const {
	const auto iterator = std::find_if(this->mods_.begin(), this->mods_.end(), [&localId](const std::shared_ptr<UGCMod>& mod) {
		return mod->localId == localId;
		});

	return iterator != this->mods_.end() ? *iterator : nullptr;
}

[[nodiscard]] std::shared_ptr<UGCMod> UGCManager::getModByFileId(PublishedFileId_t fileId) const {
	const auto iterator = this->fileIdToLocalId_.find(fileId);
	if (iterator == this->fileIdToLocalId_.end())
		return nullptr;

	return this->getModByLocalId(iterator->second);
}