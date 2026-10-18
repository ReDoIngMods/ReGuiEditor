
#pragma once

#include "ImGuiFontManager.hpp"

#include <ImGui/imgui.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

using DebugTextID = uint32_t;

class DebugTextStream {
	std::vector<std::string> stream_;
public:
	void print(const char* format, ...) {
		va_list args;
		va_start(args, format);

		char buffer[1024];
		vsnprintf(buffer, sizeof(buffer), format, args);

		va_end(args);

		this->stream_.push_back(buffer);
	}

	void write(const char* str) {
		this->stream_.push_back(str);
	}

	void clear() {
		this->stream_.clear();
	}

	const std::vector<std::string>& getLines() const {
		return this->stream_;
	}
};

class DebugTextHandler {
private:
	struct Entry {
		DebugTextID id;
		std::string name;
		std::function<void(DebugTextStream&)> textFunction;
		bool enabled = true;
	};

	std::vector<Entry> entries_;
	DebugTextID nextId_ = 1;

	DebugTextStream stream_;

	Entry* findEntry(DebugTextID id) {
		for (auto& entry : this->entries_) {
			if (entry.id == id)
				return &entry;
		}

		return nullptr;
	}

	const Entry* findEntry(DebugTextID id) const {
		for (const auto& entry : this->entries_) {
			if (entry.id == id)
				return &entry;
		}

		return nullptr;
	}
public:
	bool renderingEnabled = true;

	void initialize();

	DebugTextID addEntry(std::string name, std::function<void(DebugTextStream&)> textFunction) {
		if (this->nextId_ == 0) {
			CONSOLE_ERROR("Debug text entry ID overflow. Cannot add more entries.");
			return 0;
		}

		const DebugTextID createdId = this->nextId_++;
		this->entries_.emplace_back(createdId, std::move(name), std::move(textFunction), true);

		return createdId;
	}

	bool removeEntry(DebugTextID id) {
		for (auto iterator = this->entries_.begin(); iterator != this->entries_.end(); ++iterator) {
			if (iterator->id == id) {
				this->entries_.erase(iterator);
				return true;
			}
		}

		CONSOLE_WARNING("Debug text entry with ID '%u' does not exist.", static_cast<unsigned int>(id));
		return false;
	}

	bool isEntryEnabled(DebugTextID id) const {
		const Entry* entry = this->findEntry(id);
		return entry && entry->enabled;
	}

	bool setEntryEnabled(DebugTextID id, bool enabled) {
		Entry* entry = this->findEntry(id);
		if (!entry) {
			CONSOLE_ERROR("Debug text entry with ID '%u' does not exist.", static_cast<unsigned int>(id));
			return false;
		}

		entry->enabled = enabled;
		return true;
	}

	void renderDebugText() {
		if (!this->renderingEnabled || this->entries_.empty())
			return;

		this->stream_.clear();

		bool firstEntry = true;
		for (const auto& entry : this->entries_) {
			if (!entry.enabled)
				continue;

			if (!firstEntry)
				this->stream_.write("\n");

			firstEntry = false;

			this->stream_.print("=== [0x%02X] %s ===", entry.id, entry.name.c_str());
			if (entry.textFunction)
				entry.textFunction(this->stream_);
		}

		ImFont* font = ImGuiFontManager::getInstance().getFont("GeistMono-Medium");
		if (font)
			ImGui::PushFont(font, 0.0f);

		const std::vector<std::string>& lines = this->stream_.getLines();
		const ImVec2 windowPos = ImGui::GetWindowPos();

		ImDrawList* windowDrawList = ImGui::GetWindowDrawList();

		float cursorY = 0.f;
		for (const auto& line : lines) {
			const ImVec2 textSize = ImGui::CalcTextSize(line.c_str());

			const ImVec2 rectMin = {
				windowPos.x,
				windowPos.y + cursorY
			};

			const ImVec2 rectMax = {
				windowPos.x + textSize.x,
				windowPos.y + cursorY + textSize.y
			};

			windowDrawList->AddRectFilled(rectMin, rectMax, 0x80000000);
			windowDrawList->AddText(rectMin, 0xFFFFFFFF, line.c_str());

			cursorY += textSize.y;
		}

		if (font)
			ImGui::PopFont();
	}
};
