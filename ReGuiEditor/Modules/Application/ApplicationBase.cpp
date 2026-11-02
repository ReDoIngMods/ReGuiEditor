#include "ApplicationBase.hpp"

#include <cstdlib>
#include <exception>

#include <FreeImage/FreeImage.h>

#include <steam/isteamapps.h>
#include <steam/isteamutils.h>
#include <steam/steam_api.h>
#include <steam/steam_api_common.h>

#include <Windows.h>

constexpr double TICKS_PER_SECOND = 1 / 40.0;

int ReGuiEditor::ApplicationBase::runStart(const wchar_t* commandLine) {
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, L"Command line: %s", commandLine);

	_putenv_s("SteamAppId", "387990");
	_putenv_s("SteamNoOverlayUIDrawing", "1");

	SteamErrMsg errMsg{};
	if (ESteamAPIInitResult result = SteamAPI_InitEx(&errMsg); result != k_ESteamAPIInitResult_OK) {
		CONSOLE_ERROR("Failed to initialize Steam API. Error Code: %d", static_cast<int>(result));

		switch (result) {
		case k_ESteamAPIInitResult_FailedGeneric:
			CONSOLE_ERROR("Steam Error: Generic initialization failure.");
			
			MessageBox(nullptr, L"Failed to initialize Steam API. Reason is not knonwn to the editor.", L"ReGuiEditor - Error", MB_ICONERROR | MB_OK);
			break;
		case k_ESteamAPIInitResult_NoSteamClient:
			CONSOLE_ERROR("Steam Error: No Steam client found.");
			
			MessageBox(nullptr, L"Failed to initialize Steam API. Please ensure that Steam is running.", L"ReGuiEditor - Error", MB_ICONERROR | MB_OK);
			break;
		case k_ESteamAPIInitResult_VersionMismatch:
			CONSOLE_ERROR("Steam Error: Matched Steam API version mismatch.");
			
			MessageBox(nullptr, L"Steam client version mismatch. Please update your Steam client.", L"ReGuiEditor - Error", MB_ICONERROR | MB_OK);
			break;
		}

		return 1;
	}

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Steam API initialized successfully.");

	if (!SteamApps()->BIsAppInstalled(SteamUtils()->GetAppID())) {
		CONSOLE_ERROR("Scrap Mechanic is not installed! Unable to use the editor.");
		
		MessageBox(nullptr, L"Scrap Mechanic is not installed. Please install the game to use the editor.", L"ReGuiEditor - Error", MB_ICONERROR | MB_OK);
		return 1;

	}

	this->isSteamInitialized_ = true;

	try {
		FreeImage_Initialise();
	} catch (const std::exception& exception) {
		CONSOLE_ERROR("Exception during FreeImage initialization: %s", exception.what());
		
		MessageBox(nullptr, L"Exception during FreeImage initialization.", L"ReGuiEditor - Error", MB_ICONERROR | MB_OK);
		return 1;
	}
	
	this->isFreeImageInitialized_ = true;

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "FreeImage library initialized successfully.");

	QueryPerformanceFrequency(&this->frequency_);
	QueryPerformanceCounter(&this->counter_);

	return 0;
}

void ReGuiEditor::ApplicationBase::runLoop() {
	SteamAPI_RunCallbacks();

	LARGE_INTEGER currentCounter;
	QueryPerformanceCounter(&currentCounter);

	this->deltaTime_ = static_cast<double>(currentCounter.QuadPart - this->counter_.QuadPart) / static_cast<double>(this->frequency_.QuadPart);
	this->counter_ = currentCounter;

	this->update();

	this->timeAccumulator_ += this->deltaTime_;

	while (this->timeAccumulator_ >= TICKS_PER_SECOND) {
		this->tick();
		this->timeAccumulator_ -= TICKS_PER_SECOND;
	}

	if (this->isRunning_)
		this->render();
}

void ReGuiEditor::ApplicationBase::runEnd() {
	if (this->isFreeImageInitialized_) {
		FreeImage_DeInitialise();
		CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "FreeImage library deinitialized successfully.");
	}

	if (this->isSteamInitialized_) {
		SteamAPI_Shutdown();
		CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Steam API shutdown successfully.");
	}
}

int ReGuiEditor::ApplicationBase::run(const wchar_t* commandLine) {
	int result = this->runStart(commandLine);

	if (result == 0 && !this->initialize()) {
		CONSOLE_ERROR(L"Failed to initialize application");
		result = 1;
	}

	while (result == 0 && this->isRunning_)
		this->runLoop();

	this->cleanup();
	this->runEnd();

	return result;
}

bool ReGuiEditor::ApplicationBase::stop(bool forced) {
	if (forced) {
		this->isRunning_ = false;
		return true;
	}

	if (this->onCloseRequest()) {
		this->isRunning_ = false;
		return true;
	}

	return false;
}