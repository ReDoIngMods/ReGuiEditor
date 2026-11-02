#pragma once
#include <Modules/Misc/Singleton.hpp>

#include <string>
#include <vector>

class LanguageManager : public ReGuiEditor::Singleton<LanguageManager> {
	std::string currentLanguage_ = "";

	struct LanguageEntry {
		std::string name;
		std::string folderName;
		std::vector<std::string> interfaceTagFiles;
	};

	std::vector<LanguageEntry> availableLanguages_;
public:
	bool initialize(const std::string& defaultLanguage = "english");

	const std::string& getCurrentLanguage() const {
		return this->currentLanguage_;
	}

	void setCurrentLanguage(const std::string& language);
};