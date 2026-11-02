#pragma once
#include <Modules/Misc/Singleton.hpp>

#include <string>

class LayoutManager : public ReGuiEditor::Singleton<LayoutManager> {
public:
	void initialize();

	bool loadLayout(const std::string& layoutName);
};