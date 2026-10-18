#pragma once
#include <MyGui/MyGUI_DataManager.h>
#include <MyGui/MyGUI_IDataStream.h>
#include <MyGui/MyGUI_Types.h>

#include <algorithm>
#include <string>
#include <vector>

class DataManager : public MyGUI::DataManager {
private:
	struct SearchDirectory {
		std::string path;
		bool recursive;
	};

	std::vector<SearchDirectory> searchDirectories_ = {
		SearchDirectory{"$GAME_DATA/Gui", false},
		SearchDirectory{"$GAME_DATA/Gui/Editor", false},
		SearchDirectory{"$GAME_DATA/Gui/Fonts", false},
		SearchDirectory{"$GAME_DATA/Gui/MyGUI_Media", false},
		SearchDirectory{"$GAME_DATA/Gui/Beacon", false},
		SearchDirectory{"$GAME_DATA/Gui/EnemyMarkers", false},
		SearchDirectory{"$GAME_DATA/Gui/Handbook", false},
		SearchDirectory{"$GAME_DATA/Gui/LostItem", false},
		SearchDirectory{"$GAME_DATA/Gui/QuestMarker", false},
		SearchDirectory{"$GAME_DATA/Gui/QuestPins", false},
		SearchDirectory{"$GAME_DATA/Gui/Waypoint", false},

		SearchDirectory{"$CHALLENGE_DATA/Gui", false},
		SearchDirectory{"$SURVIVAL_DATA/Gui" , false}
	};
public:
	MyGUI::IDataStream* getData(const std::string& _name) override;

	bool isDataExist(const std::string& _name) override;

	const MyGUI::VectorString& getDataListNames(const std::string& _pattern) override;

	const std::string& getDataPath(const std::string& _name) override;

	void addSearchDirectory(const std::string& path, bool recursive) {
		this->searchDirectories_.push_back({ path, recursive });
	}

	void removeSearchDirectory(const std::string& path) {
		auto iterator = std::remove_if(this->searchDirectories_.begin(), this->searchDirectories_.end(), [&path](const SearchDirectory& dir) -> bool {
			return dir.path == path;
		});

		this->searchDirectories_.erase(iterator, this->searchDirectories_.end());
	}
};