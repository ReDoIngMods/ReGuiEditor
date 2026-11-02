#pragma once
#include "LuaResult.hpp"
#include "LuaState.hpp"

#include <Modules/UniqueIdentifiers/Uuid.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <string>
#include <utility>

namespace ReGuiEditor {
	class LuaVM {
		ReGuiEditor::LuaState& luaState_;
		ReGuiEditor::Uuid modLocalId_;

		int environmentRef_ = LUA_NOREF;
		int instanceRef_ = LUA_NOREF;

		bool pushMethod(const char* name);
		void activate();
	public:
		LuaVM(ReGuiEditor::LuaState& luaState, const ReGuiEditor::Uuid& modLocalId) : luaState_(luaState), modLocalId_(modLocalId) {}
		~LuaVM() { this->shutdown(); }

		LuaVM(LuaVM&) = delete;
		LuaVM(LuaVM&&) = delete;
		LuaVM& operator=(LuaVM&) = delete;
		LuaVM& operator=(LuaVM&&) = delete;

		[[nodiscard]] bool initialize();
		void shutdown();

		[[nodiscard]] static ReGuiEditor::LuaVM* getActive(lua_State* state);

		void update(float deltaTime);
		void tick();

		[[nodiscard]] bool isInitialized() const { return this->environmentRef_ != LUA_NOREF; }
		[[nodiscard]] bool hasInstance() const { return this->instanceRef_ != LUA_NOREF; }
		[[nodiscard]] const ReGuiEditor::Uuid& getModLocalId() const { return this->modLocalId_; }

		[[nodiscard]] ReGuiEditor::LuaResult doFile(const std::string& path);

		[[nodiscard]] int getGlobalField(const char* name);

		[[nodiscard]] ReGuiEditor::LuaResult call(int argCount, int resultCount);
		[[nodiscard]] ReGuiEditor::LuaResult createInstance(const std::string& className);

		[[nodiscard]] bool startInstance();

		template<typename... Args>
		[[nodiscard]] ReGuiEditor::LuaResult callMethod(const char* name, int resultCount, Args&&... arguments) {
			if (!this->pushMethod(name))
				return ReGuiEditor::LuaResult::generateFailure("the instance has no method '%s'", name);

			(this->luaState_.push(std::forward<Args>(arguments)), ...);
			return this->call(static_cast<int>(sizeof...(Args)) + 1, resultCount);
		}

		template<typename... Args>
		void callHook(const char* name, Args&&... arguments) {
			if (!this->pushMethod(name))
				return;

			(this->luaState_.push(std::forward<Args>(arguments)), ...);
			this->call(static_cast<int>(sizeof...(Args)) + 1, 0).report();
		}
	};
}
