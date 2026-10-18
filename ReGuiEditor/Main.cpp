#include "Application.hpp"

#include <steam/steam_api.h>
#include <steam/steam_api_common.h>

#include <FreeImage/FreeImage.h>

#include <Windows.h>
#include <sal.h>

#include <cstdlib>

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow) {
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, L"Command line: %s", pCmdLine);

	_putenv_s("SteamAppId", "387990");
	_putenv_s("SteamNoOverlayUIDrawing", "1");

	SteamErrMsg errMsg;
	if (SteamAPI_InitEx(&errMsg) != k_ESteamAPIInitResult_OK) {
		CONSOLE_ERROR("Failed to initialize Steam API. Error Message: %s", errMsg);
		return false;
	}

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Steam API initialized successfully.");

	FreeImage_Initialise();
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "FreeImage library initialized successfully.");

	Application* application = Application::createInstance();
	if (!application->initialize()) {
		CONSOLE_ERROR(L"Failed to initialize application");

		application->destroyInstance();
		return 1;
	}

	while (application->isRunning) {
		SteamAPI_RunCallbacks();

		application->update();
		application->render();
	}

	application->isRunning = false;
	application->destroyInstance();

	FreeImage_DeInitialise();

	SteamAPI_Shutdown();

	return 0;
}