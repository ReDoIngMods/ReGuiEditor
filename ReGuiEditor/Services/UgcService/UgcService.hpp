#pragma once
#include "../Service.hpp"

#include <steam/isteamremotestorage.h>
#include <steam/isteamugc.h>
#include <steam/steam_api_common.h>

namespace ReGuiEditor {
	class UgcService : public ReGuiEditor::Service {
		struct UgcDefinition {

		};

		STEAM_CALLBACK_MANUAL(UgcService, onItemInstalled, ItemInstalled_t, itemInstalledCallback_);
		STEAM_CALLBACK_MANUAL(UgcService, onItemUnsubscribed, RemoteStoragePublishedFileUnsubscribed_t, itemUnsubscribedCallback_);
	public:
		bool initialize() override;
		void cleanup() override;
	};
}