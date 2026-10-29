#pragma once
#include <string>

#include <UTILS/Singleton.hpp>
#include <vector>

class LanguageManager : public UTILS::Singleton<LanguageManager> {
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