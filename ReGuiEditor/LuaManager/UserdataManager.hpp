#pragma once
#include <UTILS/Singleton.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T>
struct UserdataType {
	inline static const char* name = nullptr;
};

class UserdataDefinition {
	friend class UserdataManager;

	struct Entry {
		std::string name;
		lua_CFunction function;
	};

	std::string name_;
	int typeId_;

	std::vector<Entry> methods_;
	std::vector<Entry> getters_;
	std::vector<Entry> setters_;
	std::vector<Entry> metamethods_;

	lua_CFunction destructor_ = nullptr;
	bool registered_ = false;

	template <typename T>
	static int destroy(lua_State* L) {
		static_cast<T*>(lua_touserdata(L, 1))->~T();
		return 0;
	}
public:
	UserdataDefinition(std::string name, int typeId) : name_(std::move(name)), typeId_(typeId) { }

	void addMethod(const char* name, lua_CFunction function) { this->methods_.push_back({ name, function }); }
	void addGetter(const char* name, lua_CFunction function) { this->getters_.push_back({ name, function }); }
	void addSetter(const char* name, lua_CFunction function) { this->setters_.push_back({ name, function }); }
	void addMetamethod(const char* name, lua_CFunction function) { this->metamethods_.push_back({ name, function }); }

	template <typename T>
	void bind() {
		UserdataType<T>::name = this->name_.c_str();
		this->destructor_ = std::is_trivially_destructible_v<T> ? nullptr : &destroy<T>;
	}

	[[nodiscard]] const std::string& getName() const { return this->name_; }
	[[nodiscard]] int getTypeId() const { return this->typeId_; }
};

class UserdataManager : public UTILS::Singleton<UserdataManager> {
	std::vector<std::unique_ptr<UserdataDefinition>> definitions_;

	static int indexDispatch(lua_State* L);
	static int newIndexDispatch(lua_State* L);

	static void registerMetatable(lua_State* L, UserdataDefinition& definition);
public:
	UserdataManager() = default;
	~UserdataManager() = default;

	[[nodiscard]] UserdataDefinition* createDefinition(const std::string& name, int typeId);
	[[nodiscard]] UserdataDefinition* getDefinition(const std::string& name) const;

	void registerDefinitions(lua_State* L);

	template <typename T, typename... Args>
	T& create(lua_State* L, Args&&... args) const {
		T* object = new (lua_newuserdata(L, sizeof(T))) T(std::forward<Args>(args)...);

		luaL_getmetatable(L, UserdataType<T>::name);
		lua_setmetatable(L, -2);

		return *object;
	}

	template <typename T>
	[[nodiscard]] T* test(lua_State* L, int index) const {
		void* object = lua_touserdata(L, index);
		if (!object || !lua_getmetatable(L, index))
			return nullptr;

		luaL_getmetatable(L, UserdataType<T>::name);
		const bool matches = lua_rawequal(L, -1, -2) != 0;
		lua_pop(L, 2);

		return matches ? static_cast<T*>(object) : nullptr;
	}

	template <typename T>
	[[nodiscard]] T& check(lua_State* L, int index) const {
		return *static_cast<T*>(luaL_checkudata(L, index, UserdataType<T>::name));
	}
};
