#pragma once
#include <UTILS/Singleton.hpp>

#include <MyGui/MyGUI_IResource.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class ResolutionManager : public UTILS::Singleton<ResolutionManager> {
	struct Resolution {
		uint32_t width;
		uint32_t height;
		std::filesystem::path folder;
	};

	std::vector<Resolution> resolutions_;
	std::vector<std::string> resources_;

	std::vector<MyGUI::IResourcePtr> loadedResources;

	const Resolution* resolution_ = nullptr;
public:
	bool initialize();
	
	bool setResolution(uint32_t width, uint32_t height);

	uint32_t getResolutionWidth() const { return this->resolution_ ? this->resolution_->width : 0; }
	uint32_t getResolutionHeight() const { return this->resolution_ ? this->resolution_->height : 0; }
};