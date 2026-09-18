#include "Application.hpp"

#include "Filesystem/Filesystem.hpp"

#include <steam/steam_api.h>
#include <steam/steam_api_common.h>

#include <FreeImage/FreeImage.h>

#include <Windows.h>
#include <sal.h>

#include <chrono>
#include <thread>

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow) {
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, L"Command line: %s", pCmdLine);

	if (!FS::writeFile("steam_appid.txt", "387990")) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to create steam_appid.txt file.");
		return 1;
	}

	SteamErrMsg errMsg;
	if (SteamAPI_InitEx(&errMsg) != k_ESteamAPIInitResult_OK) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, "Failed to initialize Steam API. Error Message: %s", errMsg);
		return false;
	}

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "Steam API initialized successfully.");

	FreeImage_Initialise();
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "FreeImage library initialized successfully.");

	Application* application = Application::createInstance();
	if (!application->initialize()) {
		CONSOLE_LOG(LEVEL_ERROR, COLOR_RED, L"Failed to initialize application");

		application->destroyInstance();
		return 1;
	}

	while (application->isRunning) {
		SteamAPI_RunCallbacks();

		application->update();
		if (application->d3d11Device.isOccluded()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		application->render();
	}

	application->isRunning = false;
	application->destroyInstance();

	FreeImage_DeInitialise();

	SteamAPI_Shutdown();

	return 0;
}