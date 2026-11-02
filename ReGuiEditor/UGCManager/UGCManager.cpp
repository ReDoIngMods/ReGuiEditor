#include "UGCManager.hpp"

#include "UGCMod.hpp"

#include <Window/Splashscreen.hpp>

#include <Filesystem/DirectoryManager.hpp>

#include <Modules/Misc/ParallelFor.hpp>

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/isteamutils.h>
#include <steam/steamtypes.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
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

bool UGCManager::commitMod(ModCandidate& candidate) {
	const std::filesystem::path& modPath = candidate.path;
	const bool isLocal = candidate.isLocal;

	const std::shared_ptr<UGCMod> mod = std::move(candidate.mod);
	if (!mod) {
		CONSOLE_ERROR("Failed to create UGCMod from path: %s (%s)", modPath.string().c_str(), candidate.error.c_str());
		return false;
	}

	if (!ReGuiEditor::DirectoryManager::getInstance().addDirectory(mod->contentPath, modPath)) {
		CONSOLE_WARNING("Mod: %s was already loaded with localId: %s", modPath.string().c_str(), mod->localId.c_str());
		return false;
	}

	SET_SPLASH_STATUS("Loading mod: %s (%s)", mod->localId.c_str(), modPath.string().c_str());

	this->mods_.push_back(mod);
	if (mod->fileId != 0)
		this->fileIdToLocalId_[mod->fileId] = mod->localId;

	if (isLocal)
		this->localMods_.push_back(mod);
	else
		this->workshopMods_.push_back(mod);

	return true;
}

size_t UGCManager::registerMods(std::vector<ModCandidate>& candidates) {
	ReGuiEditor::parallelFor(candidates.size(), [&candidates](size_t index) {
		ModCandidate& candidate = candidates[index];
		try {
			candidate.mod = UGCMod::fromPath(candidate.path, candidate.isLocal, candidate.error);
		} catch (const std::exception& exception) {
			candidate.mod = nullptr;
			candidate.error = exception.what();
		}
	});

	size_t registered = 0;
	for (ModCandidate& candidate : candidates) {
		if (this->commitMod(candidate))
			++registered;
	}

	return registered;
}

bool UGCManager::registerMod(const std::filesystem::path& modPath, bool isLocal) {
	std::vector<ModCandidate> candidates;
	candidates.emplace_back(modPath, isLocal);

	return this->registerMods(candidates) == 1;
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

void UGCManager::loadGameDirectories() {
	struct GameDirectory {
		const char* key;
		const char* name;
	};

	static constexpr GameDirectory gameDirectories[] = {
		{ "$GAME_DATA", "Game Data" },
		{ "$SURVIVAL_DATA", "Survival Data" },
		{ "$CHALLENGE_DATA", "Challenge Data" },
		{ "$PARTICLE_EDITOR_DATA", "Particle Editor Data" }
	};

	this->gameDirectories_.clear();

	ReGuiEditor::DirectoryManager& directoryManager = ReGuiEditor::DirectoryManager::getInstance();
	for (const GameDirectory& gameDirectory : gameDirectories) {
		std::filesystem::path resolvedPath;
		if (!directoryManager.resolveDirectory(gameDirectory.key, resolvedPath))
			continue;

		std::error_code error;
		if (!std::filesystem::is_directory(resolvedPath, error))
			continue;

		const std::shared_ptr<UGCMod> mod = std::make_shared<UGCMod>();
		mod->name = gameDirectory.name;
		mod->localId = gameDirectory.key + 1; // Skip the '$'
		mod->contentPath = gameDirectory.key;
		mod->thumbnailFilepath = "$RGE_DATA/Images/thumbnail_missing.jpg";
		mod->path = std::move(resolvedPath);
		mod->version = 2;
		mod->isGameDirectory = true;

		this->gameDirectories_.push_back(mod);
	}
}

bool UGCManager::loadLocalMods() {
	std::filesystem::path modsRoot;
	if (!ReGuiEditor::DirectoryManager::getInstance().resolveDirectory("$RGE_USER_DATA/Mods", modsRoot)) {
		CONSOLE_ERROR("Failed to resolve the local mods folder.");
		return false;
	}

	std::error_code error;
	if (!std::filesystem::exists(modsRoot, error)) {
		CONSOLE_ERROR("Local mods folder does not exist: %s", modsRoot.string().c_str());
		return true;
	}

	std::vector<ModCandidate> candidates;
	for (std::filesystem::directory_iterator iterator(modsRoot, error), end; !error && iterator != end; iterator.increment(error)) {
		if (iterator->is_directory(error))
			candidates.push_back({ iterator->path(), true });
	}

	if (error) {
		CONSOLE_ERROR("Failed to list the local mods folder: %s", error.message().c_str());
		return false;
	}

	this->registerMods(candidates);
	return true;
}

void UGCManager::loadUGCMods() {
	std::vector<PublishedFileId_t> subscribedItems(SteamUGC()->GetNumSubscribedItems());
	subscribedItems.resize(SteamUGC()->GetSubscribedItems(subscribedItems.data(), static_cast<uint32_t>(subscribedItems.size())));

	std::vector<ModCandidate> candidates;
	candidates.reserve(subscribedItems.size());

	for (const PublishedFileId_t fileId : subscribedItems) {
		if (this->fileIdToLocalId_.contains(fileId))
			continue;

		std::filesystem::path itemPath;
		if (!getItemInstallPath(fileId, itemPath)) {
			CONSOLE_ERROR("Failed to get the installation path for Steam Workshop item: %llu", static_cast<unsigned long long>(fileId));
			continue;
		}

		candidates.push_back({ std::move(itemPath), false });
	}

	this->registerMods(candidates);
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
	if (!ReGuiEditor::DirectoryManager::getInstance().removeDirectory(mod->contentPath))
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
	this->loadGameDirectories();

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

	ReGuiEditor::DirectoryManager& directoryManager = ReGuiEditor::DirectoryManager::getInstance();
	for (const auto& mod : this->mods_)
		directoryManager.removeDirectory(mod->contentPath);

	this->fileIdToLocalId_.clear();
	this->gameDirectories_.clear();
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

[[nodiscard]] std::shared_ptr<UGCMod> UGCManager::getModByContentPath(const std::string& contentPath) const {
	const auto matches = [&contentPath](const std::shared_ptr<UGCMod>& mod) -> bool {
		return mod->contentPath == contentPath;
	};

	if (const auto iterator = std::find_if(this->mods_.begin(), this->mods_.end(), matches); iterator != this->mods_.end())
		return *iterator;

	if (const auto iterator = std::find_if(this->gameDirectories_.begin(), this->gameDirectories_.end(), matches); iterator != this->gameDirectories_.end())
		return *iterator;

	return nullptr;
}

[[nodiscard]] std::shared_ptr<UGCMod> UGCManager::getModByFileId(PublishedFileId_t fileId) const {
	const auto iterator = this->fileIdToLocalId_.find(fileId);
	if (iterator == this->fileIdToLocalId_.end())
		return nullptr;

	return this->getModByLocalId(iterator->second);
}