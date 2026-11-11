#pragma once
#include "JSON.hpp"

#include <concepts>
#include <string>
#include <type_traits>
#include <variant>

namespace {
	template<typename T, typename Variant>
	struct is_variant_member;

	template<typename T, typename... Ts>
	struct is_variant_member<T, std::variant<Ts...>> : std::bool_constant<(std::same_as<T, Ts> || ...)> {};

	template<typename T, typename Variant>
	concept is_variant_member_concept = is_variant_member<T, Variant>::value;
}


struct JSONSerializable {	
protected:
	template<typename T>
	[[nodiscard]] bool getValue(const JSONValue& value, T& output) {
		static_assert(is_variant_member_concept<T, JSONValue::variant_type>, "Type T must be a member of JSONValue::variant_type");

		using D = std::decay_t<T>;

		if constexpr (std::is_same_v<D, bool>) {
			if (!value.isBoolean()) return false;
			output = static_cast<T>(value.getBoolean().value());
		} else if constexpr (std::is_same_v<D, int>) {
			if (!value.isInteger()) return false;
			output = static_cast<T>(value.getInteger().value());
		} else if constexpr (std::is_same_v<D, double>) {
			if (!value.isFloatingPoint()) return false;
			output = static_cast<T>(value.getDouble().value());
		} else if constexpr (std::is_same_v<D, std::string>) {
			if (!value.isString()) return false;
			output = static_cast<T>(value.getString().value());
		} else {
			static_assert(std::false_type::value, "Unsupported type for JSONSerializable::getValue");
		}

		return true;
	}

	template<typename T>
	[[nodiscard]] bool getValue(const JSONValue& value, const std::string& key, T& output) {
		static_assert(is_variant_member_concept<T, JSONValue::variant_type> || std::is_base_of_v<JSONSerializable, T>, "Type T must be a member of JSONValue::variant_type or a JSONSerializable");

		const auto object = value.getObject();
		if (!object) return false;
		if (!object->contains(key)) return false;

		if constexpr (std::is_base_of_v<JSONSerializable, T>)
			return output.deserialize(object->at(key));
		else
			return this->getValue(object->at(key), output);
	}

	template<typename T>
	[[nodiscard]] bool verifyValue(const JSONValue& value, const std::string& key, const T& expected) {
		static_assert(is_variant_member_concept<T, JSONValue::variant_type>, "Type T must be a member of JSONValue::variant_type");
		T actual;
		if (!this->getValue(value, key, actual)) return false;
		return actual == expected;
	}
public:
	virtual ~JSONSerializable() = default;

	virtual JSONValue serialize() const = 0;
	virtual bool deserialize(const JSONValue& value) = 0;
};