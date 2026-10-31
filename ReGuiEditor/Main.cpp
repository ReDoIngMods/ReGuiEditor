#include "Application.hpp"

#include <steam/steam_api.h>
#include <steam/steam_api_common.h>

#include <steam/isteamapps.h>
#include <steam/isteamutils.h>

#include <FreeImage/FreeImage.h>

#include <Windows.h>
#include <sal.h>

#include <cstdlib>
#include <memory>

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow) {
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, L"Command line: %s", pCmdLine);

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

	FreeImage_Initialise();
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GRAY, "FreeImage library initialized successfully.");

	{
		std::shared_ptr<Application> application = Application::createInstance();
		if (!application->initialize()) {
			CONSOLE_ERROR(L"Failed to initialize application");
			return 1;
		}

		while (application->isRunning) {
			SteamAPI_RunCallbacks();

			application->update();
			application->render();
		}

		application->isRunning = false;
	}

	FreeImage_DeInitialise();

	SteamAPI_Shutdown();

	return 0;
}