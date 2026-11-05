#include "HotReloadable.hpp"

#include <Modules/Misc/Expected.hpp>

#include <Filesystem/DirectoryManager.hpp>
#include <Filesystem/FilesystemManager.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>

bool ReGuiEditor::HotReloadable::updateLastWriteTime() {
	ReGuiEditor::FilesystemManager& filesystemManager = ReGuiEditor::FilesystemManager::getInstance();

	time_t currentWriteTime;
	if (!filesystemManager.getFileWriteTime(this->filePath_, currentWriteTime))
		return false;

	if (currentWriteTime != this->lastWriteTime_) {
		this->lastWriteTime_ = currentWriteTime;
		return true;
	}

	return false;
}

void ReGuiEditor::HotReloadable::tick() {
	if (this->updateLastWriteTime())
		this->pendingReload_ = true;

	if (this->pendingReload_) {
		this->pendingReload_ = false;
		this->onReloadCallback_();
	}
}

ReGuiEditor::Expected<std::unique_ptr<ReGuiEditor::HotReloadable>> ReGuiEditor::HotReloadable::create(const std::string& filePath, std::function<void()> onReloadCallback) {
	if (filePath.empty() || !onReloadCallback)
		return ReGuiEditor::ExpectedError("Invalid parameters");

	ReGuiEditor::FilesystemManager& filesystemManager = ReGuiEditor::FilesystemManager::getInstance();
	if (!filesystemManager.exists(filePath))
		return ReGuiEditor::ExpectedError("File does not exist");

	std::unique_ptr<ReGuiEditor::HotReloadable> hotReloadable = std::make_unique<ReGuiEditor::HotReloadable>();
	hotReloadable->filePath_ = filePath;
	hotReloadable->onReloadCallback_ = onReloadCallback;

	if (!filesystemManager.getFileWriteTime(filePath, hotReloadable->lastWriteTime_))
		return ReGuiEditor::ExpectedError("Failed to get file write time");

	auto& directoryManager = ReGuiEditor::DirectoryManager::getInstance();
	if (!directoryManager.resolveDirectory(filePath, hotReloadable->rawFilePath_))
		return ReGuiEditor::ExpectedError("Failed to resolve directory");

	return std::move(hotReloadable);
}