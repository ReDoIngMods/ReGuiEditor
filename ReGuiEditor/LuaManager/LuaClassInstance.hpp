#pragma once
#include <LuaJIT/lauxlib.h>

#include <string>

class LuaVM;

enum class LuaClassType {
	Loader,
	Addon
};

class LuaClassInstance {
	LuaVM* vm_ = nullptr;
	LuaClassType type_;
	std::string className_;
	int instanceRef_ = LUA_NOREF;

	bool pushClass();
public:
	LuaClassInstance(LuaVM* vm, LuaClassType type, const std::string& className) : vm_(vm), type_(type), className_(className) {}

	[[nodiscard]] bool create();
	[[nodiscard]] bool refresh();
	void destroy();

	[[nodiscard]] bool exists() const { return this->instanceRef_ != LUA_NOREF; }
	[[nodiscard]] LuaVM& getVM() const { return *this->vm_; }
	[[nodiscard]] LuaClassType getType() const { return this->type_; }
	[[nodiscard]] bool callMethod(const std::string& methodName, int argCount, int resultCount);
};