#pragma once
#include <Modules/Misc/Expected.hpp>

#include <filesystem>
#include <functional>
#include <string>

namespace ReGuiEditor {
	class HotReloadable {
		std::string filePath_;
		std::filesystem::path rawFilePath_;
		std::function<void()> onReloadCallback_;

		time_t lastWriteTime_;
		bool pendingReload_ = false;

		bool updateLastWriteTime();
		void tick();

		friend class HotReloadService;
	public:
		const std::string& getFilePath() const { return this->filePath_; }
		const std::filesystem::path& getRawFilePath() const { return this->rawFilePath_; }

		bool isPendingReload() const { return this->pendingReload_; }
		void forceReload() { this->pendingReload_ = true; }

		static ReGuiEditor::Expected<std::unique_ptr<ReGuiEditor::HotReloadable>> create(const std::string& filePath, std::function<void()> onReloadCallback);
	};
}