#pragma once
#include "LuaVM.hpp"

#include <UTILS/Singleton.hpp>

#include <memory>
#include <string>
#include <vector>

class LuaManager : public UTILS::Singleton<LuaManager> {
	std::vector<std::unique_ptr<LuaVM>> vms_;

	bool createVM(const std::string& modLocalId, const std::string& entryScript);
public:
	LuaManager() = default;
	~LuaManager() { this->cleanup(); }

	LuaManager(const LuaManager&) = delete;
	LuaManager& operator=(const LuaManager&) = delete;
	LuaManager(LuaManager&&) = delete;
	LuaManager& operator=(LuaManager&&) = delete;

	bool initialize();
	void cleanup();

	void update(float deltaTime);

	[[nodiscard]] LuaVM* getVM(const std::string& modLocalId) const;
};
