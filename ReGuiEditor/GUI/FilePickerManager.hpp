#pragma once
#include "Views/FilePickerView.hpp" // VCIC-Excluded

#include <UTILS/Singleton.hpp>

#include "FilePickerRequest.hpp"

class FilePickerManager : public UTILS::Singleton<FilePickerManager> {
	FilePickerRequest currentRequest_;
	bool isActive_ = false;

	friend class FilePickerView;
public:
	void initialize();

	bool isActive() const;
	bool sendRequest(const FilePickerRequest& request);
};