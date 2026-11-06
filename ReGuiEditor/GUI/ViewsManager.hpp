#pragma once
#include "View.hpp"

#include <UTILS/Profiler.hpp>
#include <UTILS/Singleton.hpp>
#include <UTILS/Strings.hpp>

#include <type_traits>
#include <vector>

class ViewsManager : public UTILS::Singleton<ViewsManager> {
	struct ViewInstance {
		View* view;
		const char* name;
	};

	std::vector<ViewInstance> views_;
public:
	ViewsManager() {
		CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Initializing ViewsManager...");
	}

	~ViewsManager() {
		CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Deinitializing ViewsManager...");

		for (auto& viewInstance : this->views_) {
			viewInstance.view->deinitialize();
			delete viewInstance.view;
		}

		this->views_.clear();
	}

	template <typename T>
	void registerView(const char* name) {
		static_assert(std::is_base_of_v<View, T>, "T must be derived from View");

		View* view = new T;
		if (!view->initialize()) {
			CONSOLE_ERROR("Failed to initialize view: {}", name);

			delete view;
			return;
		}

		this->views_.emplace_back(view, name);
	}

	template<typename T>
	T* getView() {
		static_assert(std::is_base_of_v<View, T>, "T must be derived from View");
		
		for (auto& viewInstance : this->views_) {
			if (T* castedView = dynamic_cast<T*>(viewInstance.view))
				return castedView;
		}
	
		return nullptr;
	}

	void render() {
		for (auto& viewInstance : this->views_) {
			PROFILE_SCOPE(UTILS::formatString("View - %s", viewInstance.name));
			viewInstance.view->render();
		}
	}

	void renderMenu() {
		for (auto& viewInstance : this->views_)
			viewInstance.view->renderMenu();
	}

	void tick() {
		for (auto& viewInstance : this->views_)
			viewInstance.view->tick();
	}

	void update(double deltaTime) {
		for (auto& viewInstance : this->views_)
			viewInstance.view->update(deltaTime);
	}
};