#include "HotReloadService.hpp"

#include "HotReloadable.hpp"

#include <Modules/Misc/Expected.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

bool ReGuiEditor::HotReloadService::addHotReloadable(const std::string& filePath, std::function<void()> onReloadCallback) {
	auto hotReloadable = ReGuiEditor::HotReloadable::create(filePath, onReloadCallback);
	if (!hotReloadable) {
		CONSOLE_ERROR("Failed to add hot reloadable for file '{}': {}", filePath, hotReloadable.get_error_message());
		return false;
	}

	std::lock_guard<std::mutex> lock(this->hotReloadablesMutex_);
	this->hotReloadables_.emplace_back(std::move(hotReloadable.value()));
	return true;
}

bool ReGuiEditor::HotReloadService::removeHotReloadable(const std::string& filePath) {
	std::lock_guard<std::mutex> lock(this->hotReloadablesMutex_);
	auto iterator = std::find_if(this->hotReloadables_.begin(), this->hotReloadables_.end(), [&filePath](const std::unique_ptr<ReGuiEditor::HotReloadable>& hotReloadable) {
		return hotReloadable->getFilePath() == filePath;
	});

	if (iterator != this->hotReloadables_.end()) {
		this->hotReloadables_.erase(iterator);
		return true;
	}

	return false;
}

void ReGuiEditor::HotReloadService::tick() {
	std::lock_guard<std::mutex> lock(this->hotReloadablesMutex_);
	for (const auto& hotReloadable : this->hotReloadables_)
		hotReloadable->tick();
}