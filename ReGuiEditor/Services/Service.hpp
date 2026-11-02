#pragma once

namespace ReGuiEditor {
	enum class ServiceInitStage {
		PreInit,
		PostInit,
	};

	class Service {
	public:
		virtual ~Service() = default;

		virtual bool canInitialize() const { return true; }
		virtual bool initialize() { return true;  };
		virtual void cleanup() {};

		virtual void update(double deltaTime) {};
		virtual void tick() {};
		virtual void render() {};

		virtual ReGuiEditor::ServiceInitStage getInitStage() const { return ReGuiEditor::ServiceInitStage::PreInit; }
		virtual const char* getServiceName() const { return "UnnamedService"; }
	};
}