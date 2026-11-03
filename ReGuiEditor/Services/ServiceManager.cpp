#include "ServiceManager.hpp"

#include "Service.hpp"

#include <Modules/Misc/Profiler.hpp>

#include <Application.hpp>

#include <memory>
#include <utility>
#include <vector>

bool ReGuiEditor::ServiceManager::isServiceRegistered(const char* serviceName) const {
	for (const auto& service : this->services_) {
		if (service->getServiceName() == serviceName)
			return true;
	}

	return false;
}

bool ReGuiEditor::ServiceManager::initializeServices(ReGuiEditor::ServiceInitStage stage) {
	PROFILE_FUNCTION();
	
	const char* stageName = nullptr;
	switch (stage) {
	case ReGuiEditor::ServiceInitStage::PreInit: stageName = "PreInit"; break;
	case ReGuiEditor::ServiceInitStage::PostInit: stageName = "PostInit"; break;
	default:
		CONSOLE_ERROR("Unknown service initialization stage: %d", static_cast<int>(stage));
		return false;
	}

	CONSOLE_LOG(LEVEL_SERVICE, COLOR_GRAY, "Initializing services for stage: %s...", stageName);

	std::vector<std::unique_ptr<ReGuiEditor::Service>> remainingServices;
	for (size_t index = this->pendingServices_.size(); index-- > 0;) {
		if (this->pendingServices_[index]->getInitStage() != stage)
			continue;

		remainingServices.push_back(std::move(this->pendingServices_[index]));
		this->pendingServices_.erase(this->pendingServices_.begin() + index);
	}

	while (!remainingServices.empty()) {
		bool initializedAny = false;
		
		auto end = remainingServices.end();
		for (auto iterator = remainingServices.begin(); iterator != end; ) {
			ReGuiEditor::Service* service = iterator->get();
			if (!service->canInitialize()) {
				++iterator;
				continue;
			}

			CONSOLE_LOG(LEVEL_SERVICE, COLOR_GRAY, "Initializing service: %s", service->getServiceName());
			
			{
				PROFILE_SCOPE("%s::initialize", service->getServiceName());
				if (!service->initialize()) {
					CONSOLE_ERROR("Failed to initialize service: %s", service->getServiceName());
					return false;
				}
			}
			
			this->services_.push_back(std::move(*iterator));
			
			iterator = remainingServices.erase(iterator);
			end = remainingServices.end();
		
			initializedAny = true;
		}

		if (!initializedAny) {
			CONSOLE_ERROR("Deadlock detected while initializing services for stage %s. Remaining services cannot be initialized due to unmet dependencies.", stageName);
			for (const auto& service : remainingServices)
				CONSOLE_ERROR("Service: %s cannot be initialized.", service->getServiceName());
		
			return false;
		}
	}

	return true;
}

void ReGuiEditor::ServiceManager::callUpdate() {
	PROFILE_FUNCTION();

	const double deltaTime = ReGuiEditor::getApp().getDeltaTime();

	for (const auto& service : this->services_) {
		PROFILE_SCOPE("%s::update", service->getServiceName());
		service->update(deltaTime);
	}
}

void ReGuiEditor::ServiceManager::callTick() {
	PROFILE_FUNCTION();

	for (const auto& service : this->services_) {
		PROFILE_SCOPE("%s::tick", service->getServiceName());
		service->tick();
	}
}

void ReGuiEditor::ServiceManager::callRender() {
	PROFILE_FUNCTION();

	for (const auto& service : this->services_) {
		PROFILE_SCOPE("%s::render", service->getServiceName());
		service->render();
	}
}