#pragma once
#include "../Service.hpp"

#include "LuaState.hpp"
#include "LuaVM.hpp"

#include <Modules/UniqueIdentifiers/Uuid.hpp>

#include <memory>
#include <vector>

namespace ReGuiEditor {
	class LuaService : public Service {
		std::unique_ptr<LuaState> luaState_ = nullptr;
		std::vector<std::unique_ptr<LuaVM>> virtualMachines_;

		bool defineEnvironment();
	public:
		~LuaService() override { this->cleanup(); }

		bool initialize() override;
		void cleanup() override;

		void update(double deltaTime) override;
		void tick() override;

		LuaVM* createVirtualMachine(const ReGuiEditor::Uuid& modLocalId);
		[[nodiscard]] LuaVM* getVirtualMachine(const ReGuiEditor::Uuid& modLocalId) const;

		LuaState& getState() { return *this->luaState_; }

		ReGuiEditor::ServiceInitStage getInitStage() const override { return ReGuiEditor::ServiceInitStage::PostInit; }
		const char* getServiceName() const override { return "LuaService"; }
	};
}
