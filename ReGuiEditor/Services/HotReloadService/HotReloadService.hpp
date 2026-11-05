#pragma once
#include "../Service.hpp"

#include "HotReloadable.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ReGuiEditor {
	class HotReloadService : public ReGuiEditor::Service {
		std::vector<std::unique_ptr<ReGuiEditor::HotReloadable>> hotReloadables_;
		std::mutex hotReloadablesMutex_;
	public:
		bool addHotReloadable(const std::string& filePath, std::function<void()> onReloadCallback);
		bool removeHotReloadable(const std::string& filePath);

		void tick() override;
	};
}