#pragma once
#include "LuaResult.hpp"

#include <Modules/UniqueIdentifiers/Uuid.hpp>

#include <LuaJIT/lauxlib.h>
#include <LuaJIT/lua.h>

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace ReGuiEditor {
	enum class LuaType : int {
		None          = LUA_TNONE,
		Nil           = LUA_TNIL,
		Boolean       = LUA_TBOOLEAN,
		LightUserdata = LUA_TLIGHTUSERDATA,
		Number        = LUA_TNUMBER,
		String        = LUA_TSTRING,
		Table         = LUA_TTABLE,
		Function      = LUA_TFUNCTION,
		Userdata      = LUA_TUSERDATA,
		Thread        = LUA_TTHREAD
	};

	struct LuaError : std::runtime_error {
		using std::runtime_error::runtime_error;
	};

	using LuaCallable = std::function<int(lua_State*)>;

	namespace detail {
		template<typename>
		inline constexpr bool alwaysFalse = false;

		template<typename>
		inline constexpr bool isStdFunction = false;

		template<typename Result, typename... Args>
		inline constexpr bool isStdFunction<std::function<Result(Args...)>> = true;

		template<typename T>
		inline constexpr bool isLuaInteger = std::is_integral_v<T> && !std::is_same_v<T, bool>;

		template<typename T>
		bool fitsInteger(lua_Number value) {
			const double limit = std::ldexp(1.0, std::numeric_limits<T>::digits);
			const double lower = std::is_signed_v<T> ? -limit : 0.0;

			return std::trunc(value) == value && value >= lower && value < limit;
		}

		struct LuaRef {
			lua_State* state = nullptr;
			int referenceIndex = LUA_NOREF;

			LuaRef(lua_State* state, int index) : state(state) {
				lua_pushvalue(state, index);
				this->referenceIndex = luaL_ref(state, LUA_REGISTRYINDEX);
			}

			~LuaRef() {
				luaL_unref(this->state, LUA_REGISTRYINDEX, this->referenceIndex);
			}

			LuaRef(LuaRef&) = delete;
			LuaRef& operator=(LuaRef&) = delete;

			void push() const {
				lua_rawgeti(this->state, LUA_REGISTRYINDEX, this->referenceIndex);
			}
		};
	}

	class LuaState {
		lua_State* state_ = nullptr;
		bool isOwner_ = false;

		explicit LuaState(lua_State* state) : state_(state) {}
	public:
		LuaState() = default;
		~LuaState();

		LuaState(const LuaState&) = delete;
		LuaState(LuaState&&) = delete;
		LuaState& operator=(const LuaState&) = delete;
		LuaState& operator=(LuaState&&) = delete;

		bool initialize();

		// Loads the file as a chunk running inside environmentRef and leaves it on the stack.
		[[nodiscard]] LuaResult loadFile(const std::string& path, const ReGuiEditor::Uuid& modLocalId, int environmentRef);
		[[nodiscard]] LuaResult doFile(const std::string& path, const ReGuiEditor::Uuid& modLocalId, int environmentRef);

		// Protected call of the function and its arguments on top of the stack.
		[[nodiscard]] LuaResult call(int argCount, int resultCount);

		static LuaState wrapState(lua_State* state) { return LuaState(state); }

		lua_State* getState() const { return this->state_; }

		int getTop() const { return lua_gettop(this->state_); }
		void setTop(int index) { lua_settop(this->state_, index); }
		void pop(int count = 1) { lua_pop(this->state_, count); }

		template<typename T>
		void push(T&& value) {
			using DecayedValue = std::decay_t<T>;
			if constexpr (std::is_same_v<DecayedValue, std::nullptr_t>) {
				lua_pushnil(this->state_);
			} else if constexpr (std::is_same_v<DecayedValue, bool>) {
				lua_pushboolean(this->state_, value);
			} else if constexpr (std::is_arithmetic_v<DecayedValue>) {
				lua_pushnumber(this->state_, static_cast<lua_Number>(value));
			} else if constexpr (std::is_same_v<DecayedValue, std::string> || std::is_same_v<DecayedValue, std::string_view>) {
				lua_pushlstring(this->state_, value.data(), value.size());
			} else if constexpr (std::is_same_v<DecayedValue, const char*> || std::is_same_v<DecayedValue, char*>) {
				lua_pushstring(this->state_, value);
			} else if constexpr (std::is_same_v<DecayedValue, lua_CFunction>) {
				lua_pushcfunction(this->state_, value);
			} else if constexpr (std::is_same_v<DecayedValue, void*>) {
				lua_pushlightuserdata(this->state_, value);
			} else if constexpr (detail::isStdFunction<DecayedValue>) {
				this->pushFunction(std::forward<T>(value));
			} else {
				static_assert(detail::alwaysFalse<DecayedValue>, "LuaState::push: unsupported type");
			}
		}

		LuaType typeOf(int index) const { return static_cast<LuaType>(lua_type(this->state_, index)); }
		const char* typeName(int index) const { return luaL_typename(this->state_, index); }

		bool isNone(int index) const { return this->typeOf(index) == LuaType::None; }
		bool isNil(int index) const { return this->typeOf(index) == LuaType::Nil; }
		bool isNoneOrNil(int index) const { return lua_type(this->state_, index) <= 0; }

		template<typename T>
		bool is(int index) const {
			using DecayedValue = std::decay_t<T>;
			if constexpr (std::is_same_v<DecayedValue, std::nullptr_t>) {
				return this->isNil(index);
			} else if constexpr (std::is_same_v<DecayedValue, bool>) {
				return this->typeOf(index) == LuaType::Boolean;
			} else if constexpr (detail::isLuaInteger<DecayedValue>) {
				return this->typeOf(index) == LuaType::Number && detail::fitsInteger<DecayedValue>(lua_tonumber(this->state_, index));
			} else if constexpr (std::is_floating_point_v<DecayedValue>) {
				return this->typeOf(index) == LuaType::Number;
			} else if constexpr (std::is_same_v<DecayedValue, std::string> || std::is_same_v<DecayedValue, std::string_view> || std::is_same_v<DecayedValue, const char*>) {
				return this->typeOf(index) == LuaType::String;
			} else if constexpr (std::is_same_v<DecayedValue, lua_CFunction>) {
				return lua_iscfunction(this->state_, index) != 0;
			} else if constexpr (std::is_same_v<DecayedValue, void*>) {
				return this->typeOf(index) == LuaType::LightUserdata;
			} else if constexpr (detail::isStdFunction<DecayedValue>) {
				return this->typeOf(index) == LuaType::Function;
			} else {
				static_assert(detail::alwaysFalse<DecayedValue>, "LuaState::is: unsupported type");
			}
		}

		template<typename T>
		std::optional<std::decay_t<T>> get(int index) const {
			using DecayedValue = std::decay_t<T>;
			if (!this->is<DecayedValue>(index))
				return std::nullopt;

			if constexpr (std::is_same_v<DecayedValue, bool>) {
				return lua_toboolean(this->state_, index) != 0;
			} else if constexpr (std::is_arithmetic_v<DecayedValue>) {
				return static_cast<DecayedValue>(lua_tonumber(this->state_, index));
			} else if constexpr (std::is_same_v<DecayedValue, std::string>) {
				std::size_t length = 0;
				const char* data = lua_tolstring(this->state_, index, &length);

				return std::string(data, length);
			} else if constexpr (std::is_same_v<DecayedValue, lua_CFunction>) {
				return lua_tocfunction(this->state_, index);
			} else if constexpr (std::is_same_v<DecayedValue, void*>) {
				return lua_touserdata(this->state_, index);
			} else if constexpr (detail::isStdFunction<DecayedValue>) {
				return this->toFunction(index, static_cast<DecayedValue*>(nullptr));
			} else {
				static_assert(detail::alwaysFalse<DecayedValue>, "LuaState::get: unsupported type (use std::string)");
			}
		}

		template<typename T>
		std::optional<std::decay_t<T>> popValue() {
			auto result = this->get<T>(-1);
			this->pop();

			return result;
		}
	private:
		void pushCallable(LuaCallable callable);

		template<typename T>
		T readArgument(int index) const {
			auto value = this->get<T>(index);
			if (!value)
				throw LuaError("bad argument #" + std::to_string(index) + " (unexpected type '" + this->typeName(index) + "')");

			return std::move(*value);
		}

		template<typename Result, typename... Args, std::size_t... Indices>
		static int invoke([[maybe_unused]] LuaState& lua, const std::function<Result(Args...)>& callback, std::index_sequence<Indices...>) {
			[[maybe_unused]] std::tuple<std::decay_t<Args>...> arguments {
				lua.readArgument<std::decay_t<Args>>(static_cast<int>(Indices) + 1)...
			};

			if constexpr (std::is_void_v<Result>) {
				callback(std::forward<Args>(std::get<Indices>(arguments))...);
				return 0;
			} else {
				lua.push(callback(std::forward<Args>(std::get<Indices>(arguments))...));
				return 1;
			}
		}

		template<typename Result, typename... Args>
		void pushFunction(std::function<Result(Args...)> callback) {
			this->pushCallable([callback = std::move(callback)](lua_State* L) -> int {
				return LuaState::invoke(LuaState::wrapState(L), callback, std::index_sequence_for<Args...>{});
			});
		}

		template<typename Result, typename... Args>
		std::function<Result(Args...)> toFunction(int index, std::function<Result(Args...)>*) const {
			static_assert(!std::is_reference_v<Result>, "LuaState: std::function must not return a reference");

			auto reference = std::make_shared<detail::LuaRef>(this->state_, index);
			return [reference](Args... arguments) -> Result {
				LuaState lua = LuaState::wrapState(reference->state);
				reference->push();

				(lua.push(std::forward<Args>(arguments)), ...);

				constexpr int resultCount = std::is_void_v<Result> ? 0 : 1;
				if (lua_pcall(reference->state, static_cast<int>(sizeof...(Args)), resultCount, 0) != 0) {
					std::string message = lua.get<std::string>(-1).value_or("unknown Lua error");
					lua.pop();

					throw LuaError(std::move(message));
				}

				if constexpr (!std::is_void_v<Result>) {
					auto result = lua.popValue<Result>();
					if (!result)
						throw LuaError("unexpected return type from Lua function");

					return std::move(*result);
				}
			};
		}
	};
}
