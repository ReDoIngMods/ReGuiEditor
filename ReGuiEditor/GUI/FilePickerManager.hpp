#pragma once
#include "Views/FilePickerView.hpp" // VCIC-Excluded

#include <Modules/Misc/Singleton.hpp>

#include "FilePickerRequest.hpp"

namespace ReGuiEditor {
	class FilePickerManager : public ReGuiEditor::Singleton<FilePickerManager> {
		ReGuiEditor::FilePickerRequest currentRequest_;
		bool isActive_ = false;

		friend class FilePickerView;
	public:
		void initialize();

		bool isActive() const;
		bool sendRequest(const ReGuiEditor::FilePickerRequest& request);
	};
}