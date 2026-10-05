#pragma once
#include <UTILS/Singleton.hpp>

#include "BlurrySubSkin.hpp"

#include <MyGui/MyGUI_CommonStateInfo.h>
#include <MyGui/MyGUI_FactoryManager.h>

#include <rend/Resources/ShaderResourceView.hpp>

#include <memory>
#include <vector>

class BlurrySubSkinRegistry : public UTILS::Singleton<BlurrySubSkinRegistry> {
	std::vector<BlurrySubSkin*> instances_;
public:
	BlurrySubSkinRegistry() {
		MyGUI::FactoryManager& factoryManager = MyGUI::FactoryManager::getInstance();
		factoryManager.registerFactory<BlurrySubSkin>("BasisSkin", "BlurrySubSkin");
		factoryManager.registerFactory<MyGUI::SubSkinStateInfo>("BasisSkin/State", "BlurrySubSkin");
	}

	BlurrySubSkinRegistry(const BlurrySubSkinRegistry&) = delete;
	BlurrySubSkinRegistry& operator=(const BlurrySubSkinRegistry&) = delete;
	BlurrySubSkinRegistry(BlurrySubSkinRegistry&&) = delete;
	BlurrySubSkinRegistry& operator=(BlurrySubSkinRegistry&&) = delete;
	
	void addInstance(BlurrySubSkin* instance) {
		this->instances_.push_back(instance);
	}

	void removeInstance(BlurrySubSkin* instance) {
		this->instances_.erase(std::remove(this->instances_.begin(), this->instances_.end(), instance), this->instances_.end());
	}

	void updateAllInstances(const rend::ShaderResourceView* backdropView, int canvasWidth, int canvasHeight) {
		for (BlurrySubSkin* instance : this->instances_)
			instance->updateComposite(backdropView, canvasWidth, canvasHeight);
	}
};
