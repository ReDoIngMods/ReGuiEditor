#pragma once
#include <Windows.h>
#include <sal.h>

namespace ReGuiEditor {
	class ApplicationBase {
	protected:
		double deltaTime_ = 0.0;
		double timeAccumulator_ = 0.0;
		bool isRunning_ = true;

		bool isSteamInitialized_ = false;
		bool isFreeImageInitialized_ = false;

		LARGE_INTEGER counter_{};
		LARGE_INTEGER frequency_{};

		int runStart(const wchar_t* commandLine);
		void runLoop();
		void runEnd();
	public:
		virtual ~ApplicationBase() = default;

		// NOLINTNEXTLINE
		template <typename T> static T& getInstance();
		
		int run(const wchar_t* commandLine);

		bool isRunning() const { return this->isRunning_; }
		void setRunning(bool running) { this->isRunning_ = running; }

		bool stop(bool forced = false);

		double getDeltaTime() const { return this->deltaTime_; }

		virtual bool initialize() = 0;
		virtual void cleanup() {};

		virtual void update() {};
		virtual void tick() {};
		virtual void render() {};

		virtual bool onCloseRequest() { return true; }
		virtual void onWindowResize(int width, int height) {};
	};
}

#define REGUIEDITOR_CREATE_APPLICATION_DEFINITION(klass) \
	template <> \
	klass& ReGuiEditor::ApplicationBase::getInstance<klass>() { \
		static klass instance; \
		return instance; \
	} \
	int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int) { \
		return ReGuiEditor::ApplicationBase::getInstance<klass>().run(lpCmdLine); \
	}