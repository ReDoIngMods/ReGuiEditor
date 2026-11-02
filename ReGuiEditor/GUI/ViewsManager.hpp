#pragma once
#include "View.hpp"

#include <Modules/Misc/Singleton.hpp>

#include <type_traits>
#include <vector>

namespace ReGuiEditor {
	class ViewsManager : public ReGuiEditor::Singleton<ViewsManager> {
		struct ViewInstance {
			ReGuiEditor::View* view;
			const char* name;
		};

		std::vector<ViewInstance> views_;

		bool registerViewInternal(const char* name, ReGuiEditor::View* view);
	public:
		ViewsManager();
		~ViewsManager();

		template <typename T>
		void registerView(const char* name) {
			static_assert(std::is_base_of_v<ReGuiEditor::View, T>, "T must be derived from ReGuiEditor::View");

			ReGuiEditor::View* view = new T;
			if (!this->registerViewInternal(name, view))
				delete view;
		}

		template<typename T>
		T* getView() {
			static_assert(std::is_base_of_v<ReGuiEditor::View, T>, "T must be derived from ReGuiEditor::View");

			for (auto& viewInstance : this->views_) {
				if (T* castedView = dynamic_cast<T*>(viewInstance.view))
					return castedView;
			}

			return nullptr;
		}

		void render();
		void renderMenu();

		void tick();
		void update(double deltaTime);
	};
}