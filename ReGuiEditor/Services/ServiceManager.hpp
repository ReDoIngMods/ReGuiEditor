#pragma once
#include "Service.hpp"

#include <Modules/Misc/Singleton.hpp>

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace ReGuiEditor {
	class ServiceManager : public ReGuiEditor::Singleton<ServiceManager> {
		std::vector<std::shared_ptr<Service>> services_;
		std::vector<std::unique_ptr<Service>> pendingServices_;
	public:
		bool isServiceRegistered(const char* serviceName) const;

		template<typename T>
		bool isServiceRegistered() const {
			static_assert(std::is_base_of<Service, T>::value, "T must be derived from Service");
			
			for (const auto& service : this->services_) {
				if (dynamic_cast<T*>(service.get()) != nullptr)
					return true;
			}

			return false;
		}

		template <typename T>
		bool registerService() {
			static_assert(std::is_base_of<Service, T>::value, "T must be derived from Service");

			this->pendingServices_.push_back(std::make_unique<T>());
			return true;
		}

		template <typename T>
		static T& getService() {
			static_assert(std::is_base_of<Service, T>::value, "T must be derived from Service");
			
			ServiceManager& serviceManager = ServiceManager::getInstance();
			for (const auto& service : serviceManager.services_) {
				if (T* castedService = dynamic_cast<T*>(service.get()))
					return *castedService;
			}
		
			throw std::runtime_error("Service not found");
		}

		bool initializeServices(ReGuiEditor::ServiceInitStage stage);

		void callUpdate();
		void callTick();
		void callRender();
	};
}