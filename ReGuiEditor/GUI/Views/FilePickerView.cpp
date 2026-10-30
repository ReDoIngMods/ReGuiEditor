#include "FilePickerView.hpp"

#include <UGCManager/UGCManager.hpp>
#include <UGCManager/UGCMod.hpp>

#include <Filesystem/FilesystemManager.hpp>

#include <GUI/FilePickerManager.hpp>
#include <GUI/ImageManager.hpp>
#include <GUI/UIManager.hpp>
#include <GUI/WidgetGrid.hpp>

#include <UTILS/FuzzySearch.hpp>

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include <UTILS/Strings.hpp>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

constexpr float TOTAL_SPACE_TO_TAKE = 0.75f;
constexpr float ASPECT_RATIO_ENFORCEMENT = 16.0f / 9.0f;

constexpr const char* UNPUBLISHED_SUFFIX = "  -  Unpublished";

constexpr float ICON_ASPECT_RATIO = 16.0f / 9.0f;

constexpr float ROW_HEIGHT_FRACTION = 0.12f;
constexpr float MAX_ROW_HEIGHT_LINES = 4.f;

constexpr float MOD_LIST_WIDTH_FRACTION = 0.3f;
constexpr float MOD_LIST_MIN_WIDTH_FRAMES = 10.f;
constexpr float MOD_LIST_MAX_WIDTH_FRAMES = 15.f;

constexpr const char* OPEN_TEXT = "Open";
constexpr const char* CANCEL_TEXT = "Cancel";

constexpr const char* OUTDATED_MOD_TEXT = " (Outdated)";
constexpr const char* OUTDATED_MOD_TOOLTIP = "This mod is outdated and doesn't support JsonGui's. (Don't bother looking through here)";
constexpr ImColor OUTDATED_MOD_COLOR = IM_COL32(255, 100, 100, 255);

constexpr const char* CONTENT_PREFIX = "$CONTENT_";
constexpr size_t CONTENT_PREFIX_LENGTH = std::char_traits<char>::length(CONTENT_PREFIX);

inline static bool beginChildWidgetGrid(const char* id, const WidgetGrid& grid) {
	ImGui::SetCursorScreenPos(grid.getPosition());
	return ImGui::BeginChild(id, grid.getSize(), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
}

inline static void tooltipRow(const char* label, const std::string& value) {
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextDisabled("%s", label);
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(value.c_str());
}

inline static void drawSearchInput(const char* id, const char* hint, std::string& query) {
	char buffer[256];
	std::snprintf(buffer, sizeof(buffer), "%s", query.c_str());

	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::InputTextWithHint(id, hint, buffer, sizeof(buffer)))
		query = buffer;
}

inline static void drawEntryLabel(const ImVec2& cellMin, float rowHeight, float iconSize, const ImagePtr& icon, const char* label) {
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	const float itemInnerSpacingX = ImGui::GetStyle().ItemInnerSpacing.x;
	if (icon) {
		const ImTextureID textureId = icon->getTextureId();
		const ImVec2 iconMin(cellMin.x + itemInnerSpacingX, cellMin.y + (rowHeight - iconSize) * 0.5f);
		const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);

		drawList->AddImage(textureId, iconMin, iconMax);
	}

	const ImVec2 textPosition(cellMin.x + iconSize + (itemInnerSpacingX * 2.f), cellMin.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f);
	drawList->AddText(textPosition, ImGui::GetColorU32(ImGuiCol_Text), label);
}

inline static std::vector<size_t> rankEntries(const std::vector<FilesystemListEntry>& entries, const std::string& query) {
	std::vector<std::pair<size_t, int>> ranked;
	ranked.reserve(entries.size());

	for (size_t index = 0; index < entries.size(); ++index) {
		if (const auto score = fuzzy::score(query, entries[index].name))
			ranked.emplace_back(index, *score);
	}

	if (!query.empty())
		std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) -> bool { return a.second > b.second; });

	std::vector<size_t> order;
	order.reserve(ranked.size());

	for (const auto& entry : ranked)
		order.push_back(entry.first);

	return order;
}

void FilePickerView::drawModSearchBar(const WidgetGrid& grid) {
	if (!beginChildWidgetGrid("##modSearchBar", grid)) {
		ImGui::EndChild();
		return;
	}

	drawSearchInput("##modSearch", "Search mods...", this->searchQueryModList_);

	ImGui::EndChild();
}

void FilePickerView::showMessage(const char* message, const WidgetGrid& grid) {
	const ImVec2 textSize = ImGui::CalcTextSize(message);
	const ImVec2 center = grid.getCenterForElement(textSize);

	ImGui::GetWindowDrawList()->AddText(center, ImGui::GetColorU32(ImGuiCol_TextDisabled), message);
}

bool FilePickerView::drawImageIconButton(const char* imagePath, const char* fallback, bool isDisabled) {
	const float size = ImGui::GetFrameHeight();
	const ImVec2 buttonSize = { size, size };

	ImageManager& imageManager = ImageManager::getInstance();
	const ImagePtr icon = imageManager.findOrLoadImageSVG(imagePath, 128, 128);

	ImGui::PushID(imagePath);
	const bool clicked = ImGui::InvisibleButton("##imageButton", buttonSize);

	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	const ImGuiStyle& style = ImGui::GetStyle();
	if (ImGui::IsItemActive())
		drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_ButtonActive), style.FrameRounding);
	else if (ImGui::IsItemHovered())
		drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_ButtonHovered), style.FrameRounding);

	if (icon) {
		const float iconSize = size * 0.6f;
		const ImVec2 minImage = {
			min.x + (size - iconSize) * 0.5f,
			min.y + (size - iconSize) * 0.5f
		};

		const ImVec2 maxImage = {
			minImage.x + iconSize,
			minImage.y + iconSize
		};

		if (isDisabled) {
			const ImVec2 uv0 = { 0.f, 0.f };
			const ImVec2 uv1 = { 1.f, 1.f };

			drawList->AddImage(icon->getTextureId(), minImage, maxImage, uv0, uv1, ImGui::GetColorU32(ImGuiCol_TextDisabled));
		} else {
			drawList->AddImage(icon->getTextureId(), minImage, maxImage);
		}
	} else {
		const ImVec2 textSize = ImGui::CalcTextSize(fallback);
		const ImVec2 textPos = {
			min.x + (size - textSize.x) * 0.5f,
			min.y + (size - textSize.y) * 0.5f
		};

		drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), fallback);
	}

	ImGui::PopID();
	return !isDisabled && clicked;
}

void FilePickerView::drawModListEntries(const char* header, const std::vector<std::shared_ptr<UGCMod>>& mods) {
	const ImGuiStyle& style = ImGui::GetStyle();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	const float lineHeight = ImGui::GetTextLineHeight();
	const float iconGap = style.ItemInnerSpacing.x;

	const float minRowHeight = lineHeight * 2.f + style.FramePadding.y * 2.f;
	const float maxRowHeight = lineHeight * MAX_ROW_HEIGHT_LINES;
	const float rowHeight = std::max(minRowHeight, std::min(ImGui::GetWindowHeight() * ROW_HEIGHT_FRACTION, maxRowHeight));

	const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
	const ImU32 subtextColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);

	ImGui::SeparatorText(header);

	std::vector<std::pair<const std::shared_ptr<UGCMod>*, int>> visibleMods;
	visibleMods.reserve(mods.size());

	const std::string& query = this->searchQueryModList_;
	for (const auto& mod : mods) {
		if (mod->type != UGCModType::BlocksAndParts && mod->type != UGCModType::CustomGame)
			continue;

		if (const auto score = fuzzy::score(query, mod->name))
			visibleMods.emplace_back(&mod, *score);
	}

	std::stable_sort(visibleMods.begin(), visibleMods.end(), [](const auto& a, const auto& b) -> bool {
		if (a.second != b.second)
			return a.second > b.second;

		const int comparison = _stricmp((*a.first)->name.c_str(), (*b.first)->name.c_str());
		if (comparison != 0)
			return comparison < 0;

		return (*a.first)->name < (*b.first)->name;
	});

	if (visibleMods.empty()) {
		if (this->searchQueryModList_.empty())
			ImGui::TextDisabled("No mods found.");
		else
			ImGui::TextDisabled("No mods found matching the search query.");

		return;
	}

	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(visibleMods.size()), rowHeight + style.ItemSpacing.y);

	while (clipper.Step()) {
		for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
			const std::shared_ptr<UGCMod>& mod = *visibleMods[index].first;

			ImGui::PushID(mod.get());

			if (ImGui::Selectable("##entry", this->selectedMod_ == mod, ImGuiSelectableFlags_None, ImVec2(0.f, rowHeight))) {
				this->selectedMod_ = mod;
				this->currentFilePath_.clear();
			}

			WidgetGrid row = WidgetGrid::fromItemRect();
			const ImRect clip = row.getRectangle();
			row.pad(style.FramePadding);

			const WidgetGrid icon = row.splitLeftPixels(row.getHeight() * ICON_ASPECT_RATIO, iconGap);
			const WidgetGrid nameGrid = row.splitTopPixels(lineHeight);
			const WidgetGrid versionGrid = row.splitTopPixels(lineHeight);
			const WidgetGrid subtitleGrid = row;

			std::string subtitle = UGCModTypeToString(mod->type);
			if (mod->fileId == 0)
				subtitle += UNPUBLISHED_SUFFIX;

			ImagePtr thumbnail = mod->getThumbnail();
			if (thumbnail)
				drawList->AddImage(thumbnail->getTextureId(), icon.getPosition(), icon.getRectangle().Max);

			drawList->PushClipRect(clip.Min, clip.Max, true);
			drawList->AddText(nameGrid.getPosition(), textColor, mod->name.c_str());
			drawList->AddText(subtitleGrid.getPosition(), subtextColor, subtitle.c_str());

			const std::string versionStr = UTILS::formatString("Version: %s (%d)", mod->getVersionName(), mod->version);
			const ImVec2 versionSize = ImGui::CalcTextSize(versionStr.c_str());
			const ImVec2 versionPos = versionGrid.getPosition();

			drawList->AddText(versionGrid.getPosition(), subtextColor, versionStr.c_str());

			if (mod->isOutdated()) {
				const ImVec2 outdatedTextPosition = { versionPos.x + versionSize.x + style.ItemInnerSpacing.x, versionPos.y };
				drawList->AddText(outdatedTextPosition, OUTDATED_MOD_COLOR, OUTDATED_MOD_TEXT);
			}

			drawList->PopClipRect();

			if (ImGui::BeginItemTooltip()) {
				if (ImGui::BeginTable("##modInfo", 2, ImGuiTableFlags_SizingFixedFit)) {
					tooltipRow("Path", mod->path.string());
					tooltipRow("File ID", std::to_string(mod->fileId));
					tooltipRow("Local ID", mod->localId.c_str());

					ImGui::EndTable();
				}

				if (mod->isOutdated()) {
					ImGui::Separator();
					ImGui::TextColored(OUTDATED_MOD_COLOR, OUTDATED_MOD_TOOLTIP);
				}

				ImGui::EndTooltip();
			}

			ImGui::PopID();
		}
	}

	clipper.End();
}

void FilePickerView::drawModList(const WidgetGrid& grid) {
	if (!beginChildWidgetGrid("##modList", grid)) {
		ImGui::EndChild();
		return;
	}

	UGCManager& ugcManager = UGCManager::getInstance();
	this->drawModListEntries("Local Mods", ugcManager.getLocalMods());
	this->drawModListEntries("Workshop Mods", ugcManager.getWorkshopMods());

	ImGui::EndChild();
}

void FilePickerView::drawSearchBar(const WidgetGrid& grid) {
	if (!beginChildWidgetGrid("##searchBar", grid)) {
		ImGui::EndChild();
		return;
	}

	if (this->drawImageIconButton("$RGE_DATA/Icons/arrow-up.svg", "^", this->currentFilePath_.empty())) {
		this->currentFilePath_ = this->currentFilePath_.parent_path();
		
		this->selectedFile_.clear();
		this->selectedVisibleFile_.clear();

		this->forceRefresh_ = true;
	}

	ImGui::SameLine();

	if (this->drawImageIconButton("$RGE_DATA/Icons/refresh.svg", "R"))
		this->forceRefresh_ = true;

	ImGui::SameLine();

	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float availableWidth = ImGui::GetContentRegionAvail().x;
	const float pathWidth = availableWidth * 0.6f;

	std::string prefix;
	if (this->selectedMod_)
		prefix = "$CONTENT_" + this->selectedMod_->localId;

	std::string displayedPath = prefix;
	if (!this->currentFilePath_.empty()) {
		if (!displayedPath.empty())
			displayedPath += "/";

		displayedPath += this->currentFilePath_.generic_string();
	}

	if (!this->pathInputInitialized_ || displayedPath != this->syncedPathInput_) {
		snprintf(this->pathBuffer_, sizeof(this->pathBuffer_), "%s", displayedPath.c_str());
	
		this->syncedPathInput_ = displayedPath;
		this->pathInputInitialized_ = true;
	}

	ImGui::SetNextItemWidth(pathWidth);
	if (ImGui::InputText("##currentPath", this->pathBuffer_, sizeof(this->pathBuffer_), ImGuiInputTextFlags_EnterReturnsTrue))
		this->handlePathInput();

	ImGui::SameLine();

	char searchBuffer[256];
	snprintf(searchBuffer, sizeof(searchBuffer), "%s", this->searchQueryFileList_.c_str());

	ImGui::SetNextItemWidth(std::max(50.0f, ImGui::GetContentRegionAvail().x));
	if (ImGui::InputTextWithHint("##fileSearch", "Search files...", searchBuffer, sizeof(searchBuffer)))
		this->searchQueryFileList_ = searchBuffer;

	ImGui::EndChild();
}

void FilePickerView::handlePathInput() {
	this->pathMessage_.clear();

	const std::string enteredPath = this->pathBuffer_;
	const size_t separator = enteredPath.find_first_of("/\\");
	const std::string enteredPrefix = enteredPath.substr(0, separator);

	if (enteredPrefix.rfind(CONTENT_PREFIX, 0) != 0 || enteredPrefix.size() == CONTENT_PREFIX_LENGTH) {
		this->pathMessage_ = "Path must start with $CONTENT_<modId>.";
		return;
	}

	const std::string modId = enteredPrefix.substr(CONTENT_PREFIX_LENGTH);
	std::shared_ptr<UGCMod> matchedMod = UGCManager::getInstance().getModByLocalId(modId);
	if (!matchedMod) {
		this->pathMessage_ = "No mod found for ID '" + modId + "'.";
		return;
	}

	const std::string relativePathString = separator == std::string::npos ? "" : enteredPath.substr(separator + 1);
	const std::filesystem::path relativePath(relativePathString);
	
	const bool escapesRoot = relativePath.is_absolute() || std::any_of(relativePath.begin(), relativePath.end(), [](const std::filesystem::path& part) -> bool {
		return part == "..";
	});

	if (escapesRoot) {
		this->pathMessage_ = "Path must stay inside the selected mod.";
		return;
	}

	const std::filesystem::path candidatePath = std::filesystem::path(matchedMod->contentPath) / relativePath;
	if (!FilesystemManager::getInstance().listDirectory(candidatePath.string())) {
		this->pathMessage_ = "Folder does not exist or cannot be accessed.";
		return;
	}

	this->selectedMod_ = matchedMod;
	this->currentFilePath_ = relativePath;
	
	this->selectedFile_.clear();
	this->selectedVisibleFile_.clear();
	
	this->listedPath_.clear();
	this->forceRefresh_ = true;
}

void FilePickerView::drawFileList(const WidgetGrid& grid) {
	if (!beginChildWidgetGrid("##fileList", grid)) {
		ImGui::EndChild();
		return;
	}

	if (!this->pathMessage_.empty()) {
		this->showMessage(this->pathMessage_.c_str(), grid);
	
		ImGui::EndChild();
		return;
	}

	const std::filesystem::path currentPath = this->getFullFilePath();
	if (currentPath.empty()) {
		this->showMessage("No mod currently selected. Please select one!", grid);

		ImGui::EndChild();
		return;
	}

	if (currentPath != this->listedPath_ || this->forceRefresh_) {
		this->listedPath_ = currentPath;
		
		this->forceRefresh_ = false;

		this->selectedFile_.clear();
		this->selectedVisibleFile_.clear();

		auto listing = FilesystemManager::getInstance().listDirectory(currentPath.string());
		this->listingValid_ = listing.has_value();
		this->entries_ = listing ? std::move(*listing) : std::vector<FilesystemListEntry>{};
	}

	if (!this->listingValid_) {
		this->showMessage("Failed to list files and directories.", grid);

		ImGui::EndChild();
		return;
	}

	const std::vector<size_t> visible = rankEntries(this->entries_, this->searchQueryFileList_);
	if (visible.empty() && !this->entries_.empty()) {
		this->showMessage("No files match the search query.", grid);

		ImGui::EndChild();
		return;
	}

	if (visible.size() == 0) {
		this->showMessage("This folder is empty.", grid);

		ImGui::EndChild();
		return;
	}

	if (ImGui::BeginTable("##files", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.f);
		ImGui::TableSetupColumn("Date modified", ImGuiTableColumnFlags_WidthStretch, 1.f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.f);
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch, 1.f);
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(visible.size()));

		const float rowHeight = ImGui::GetFrameHeight();
		const float iconSize = ImGui::GetTextLineHeight();
		const uint32_t iconPixels = static_cast<uint32_t>(std::lround(iconSize));
		const ImU32 iconColor = ImGui::GetColorU32(ImGuiCol_Text);

		UIManager& uiManager = UIManager::getInstance();

		while (clipper.Step()) {
			for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
				ImGui::PushID(index);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				const ImVec2 cellPosition = ImGui::GetCursorScreenPos();

				const FilesystemListEntry& entry = this->entries_[visible[static_cast<size_t>(index)]];
				const bool selected = this->selectedVisibleFile_ == entry.name;

				const bool allowDirs = FilePickerManager::getInstance().currentRequest_.allowDirectorySelection;

				if (ImGui::Selectable("##entry", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick, { 0.f, rowHeight })) {
					if (entry.isDirectory) {
						this->selectedVisibleFile_ = entry.name;
						if (allowDirs)
							this->selectedFile_ = entry.name;

						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
							this->currentFilePath_ /= entry.name;

							this->selectedFile_.clear();
							this->selectedVisibleFile_.clear();
						}
					} else {
						this->selectedFile_ = this->selectedVisibleFile_ = entry.name;
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							this->confirmSelection();
					}
				}

				const std::string& iconPath = entry.isDirectory ? uiManager.fileIconHandler.getIconForFolder() : uiManager.fileIconHandler.getIconForFile(entry.name);
				const ImagePtr icon = iconPath.empty() ? nullptr : uiManager.imageManager->findOrLoadImageSVG(iconPath, iconPixels, iconPixels, iconColor);
				drawEntryLabel(cellPosition, rowHeight, iconSize, icon, entry.name.c_str());

				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("%s", entry.formattedWriteTime.c_str());

				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("%s", entry.isDirectory ? "File folder" : entry.fileTypename.c_str());

				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();

				if (!entry.isDirectory) {
					const uint64_t size = entry.fileSize;
					const char* suffixes[] = { "bytes", "KB", "MB", "GB", "TB" };

					int suffixIndex = 0;

					double displaySize = static_cast<double>(size);
					while (displaySize >= 1024.0 && suffixIndex < _countof(suffixes)) {
						displaySize /= 1024.0;
						++suffixIndex;
					}

					if (suffixIndex == 0)
						ImGui::TextDisabled("%llu %s", size, suffixes[suffixIndex]);
					else
						ImGui::TextDisabled("%.2f %s", displaySize, suffixes[suffixIndex]);
				}

				ImGui::PopID();
			}
		}

		clipper.End();
		ImGui::EndTable();
	}

	ImGui::EndChild();
}

void FilePickerView::drawFooter(const WidgetGrid& grid) {
	if (!beginChildWidgetGrid("##footer", grid)) {
		ImGui::EndChild();
		return;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const float spacing = style.ItemSpacing.x;

	const float openWidth = ImGui::CalcTextSize(OPEN_TEXT).x + style.FramePadding.x * 2.0f;
	const float cancelWidth = ImGui::CalcTextSize(CANCEL_TEXT).x + style.FramePadding.x * 2.0f;
	const float buttonsWidth = openWidth + cancelWidth + spacing;

	const float availableWidth = ImGui::GetContentRegionAvail().x;
	const float inputWidth = availableWidth - buttonsWidth - spacing;

	if (inputWidth > 50.0f) {
		ImGui::SetNextItemWidth(inputWidth);

		char buffer[256];
		std::snprintf(buffer, sizeof(buffer), "%s", this->selectedFile_.c_str());

		if (ImGui::InputText("##fileName", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
			this->selectedFile_ = buffer;
			this->selectedVisibleFile_ = buffer;

			this->confirmSelection();
		} else if (std::strcmp(buffer, this->selectedFile_.c_str()) != 0) {
			this->selectedFile_ = buffer;
			this->selectedVisibleFile_ = buffer;
		}

		ImGui::SameLine();
	}

	const bool hasSelectedFile = !this->selectedFile_.empty() && this->selectedMod_;
	if (!hasSelectedFile)
		ImGui::BeginDisabled();

	if (ImGui::Button(OPEN_TEXT, { openWidth, 0.0f }))
		this->confirmSelection();

	if (!hasSelectedFile)
		ImGui::EndDisabled();

	ImGui::SameLine();

	if (ImGui::Button(CANCEL_TEXT, { cancelWidth, 0.0f })) {
		FilePickerManager& filePickerManager = FilePickerManager::getInstance();
		if (filePickerManager.currentRequest_.onCancel)
			filePickerManager.currentRequest_.onCancel();

		filePickerManager.isActive_ = false;
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndChild();
}

void FilePickerView::confirmSelection() {
	if (this->selectedFile_.empty() || !this->selectedMod_)
		return;

	FilePickerManager& filePickerManager = FilePickerManager::getInstance();
	filePickerManager.isActive_ = false;

	if (filePickerManager.currentRequest_.onSelected) {
		FilePickerResult result;
		result.selectedMod = this->selectedMod_;
		result.selectedPath = (this->currentFilePath_ / this->selectedFile_).string();

		filePickerManager.currentRequest_.onSelected(result);
	}

	ImGui::CloseCurrentPopup();
}

void FilePickerView::render() {
	FilePickerManager& filePickerManager = FilePickerManager::getInstance();
	if (!filePickerManager.isActive()) {
		this->wasActive_ = false;
		return;
	}

	FilePickerRequest& request = filePickerManager.currentRequest_;

	char buffer[256];
	sprintf_s(buffer, "%s###FilePicker", request.title.c_str());

	if (!this->wasActive_) {
		this->wasActive_ = true;
		this->resetState();

		this->selectedMod_ = request.defaultMod;
		this->currentFilePath_ = request.defaultPath;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();

		float windowWidth = viewport->Size.x * TOTAL_SPACE_TO_TAKE;
		float windowHeight = viewport->Size.y * TOTAL_SPACE_TO_TAKE;

		if (windowWidth / windowHeight > ASPECT_RATIO_ENFORCEMENT)
			windowWidth = windowHeight * ASPECT_RATIO_ENFORCEMENT;
		else
			windowHeight = windowWidth / ASPECT_RATIO_ENFORCEMENT;

		const float xPos = viewport->Pos.x + (viewport->Size.x - windowWidth) * 0.5f;
		const float yPos = viewport->Pos.y + (viewport->Size.y - windowHeight) * 0.5f;

		ImGui::OpenPopup(buffer);

		ImGui::SetNextWindowPos({ xPos, yPos });
		ImGui::SetNextWindowSize({ windowWidth, windowHeight });
	}

	bool open = true;
	if (!ImGui::BeginPopupModal(buffer, &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
		if (!open) {
			this->wasActive_ = false;

			if (request.onCancel)
				request.onCancel();

			filePickerManager.isActive_ = false;
		}

		return;
	}

	const ImGuiStyle& style = ImGui::GetStyle();

	const float frameHeight = ImGui::GetFrameHeight();
	const ImVec2 spacing = ImGui::GetStyle().ItemSpacing;

	WidgetGrid windowGrid = WidgetGrid::fromCurrentWindowContentRegion();

	const float minListWidth = frameHeight * MOD_LIST_MIN_WIDTH_FRAMES;
	const float maxListWidth = frameHeight * MOD_LIST_MAX_WIDTH_FRAMES;
	const float listWidth = std::max(minListWidth, std::min(windowGrid.getWidth() * MOD_LIST_WIDTH_FRACTION, maxListWidth));

	WidgetGrid modListGrid = windowGrid.splitLeftPixels(listWidth, spacing.x);
	WidgetGrid modSearchGrid = modListGrid.splitTopPixels(frameHeight, spacing.y);
	WidgetGrid searchGrid = windowGrid.splitTopPixels(frameHeight, spacing.y);
	WidgetGrid buttonGrid = windowGrid.splitBottomPixels(frameHeight, spacing.y);
	WidgetGrid fileGrid = windowGrid;

	this->drawModSearchBar(modSearchGrid);
	this->drawModList(modListGrid);
	this->drawSearchBar(searchGrid);
	this->drawFileList(fileGrid);
	this->drawFooter(buttonGrid);

	ImGui::EndPopup();
}