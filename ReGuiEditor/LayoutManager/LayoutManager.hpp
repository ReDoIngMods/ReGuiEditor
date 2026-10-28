#pragma once
#include <UTILS/Singleton.hpp>

#include <string>

class LayoutManager : public UTILS::Singleton<LayoutManager> {
public:
	void initialize();

	bool loadLayout(const std::string& layoutName);
};