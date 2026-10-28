#pragma once
#include "../View.hpp"

#include <Filesystem/FilesystemManager.hpp>
#include <UGCManager/UGCMod.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct WidgetGrid;

class FilePickerView : public View {
	bool wasActive_ = false;

	std::string searchQueryModList_;
	std::string searchQueryFileList_;

	char pathBuffer_[1024]{};
	std::string syncedPathInput_;
	bool pathInputInitialized_ = false;
	std::string pathMessage_;

	std::shared_ptr<UGCMod> selectedMod_ = nullptr;
	std::filesystem::path currentFilePath_;

	std::filesystem::path listedPath_;

	std::vector<FilesystemListEntry> entries_;
	std::string selectedFile_;
	std::string selectedVisibleFile_;
	bool listingValid_ = false;

	bool forceRefresh_ = false;

	void resetState() {
		this->searchQueryModList_.clear();
		this->searchQueryFileList_.clear();
		this->syncedPathInput_.clear();
		this->pathInputInitialized_ = false;
		this->pathMessage_.clear();
		this->selectedMod_ = nullptr;
		this->currentFilePath_.clear();
		this->selectedFile_.clear();
		this->selectedVisibleFile_.clear();
		this->listedPath_.clear();
		this->entries_.clear();
		this->listingValid_ = false;
	}

	std::filesystem::path getFullFilePath() const {
		if (!this->selectedMod_)
			return {};

		return std::filesystem::path(this->selectedMod_->contentPath) / this->currentFilePath_;
	}

	void showMessage(const char* message, const WidgetGrid& grid);

	bool drawImageIconButton(const char* imagePath, const char* fallback, bool isDisabled = false);

	void drawModListEntries(const char* header, const std::vector<std::shared_ptr<UGCMod>>& mods);
	void drawModSearchBar(const WidgetGrid& grid);
	void drawModList(const WidgetGrid& grid);

	void drawSearchBar(const WidgetGrid& grid);
	void handlePathInput();
	void drawFileList(const WidgetGrid& grid);
	void drawFooter(const WidgetGrid& grid);

	void confirmSelection();
public:
	void render() override;
};