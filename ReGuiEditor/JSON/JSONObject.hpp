#pragma once
#include <algorithm>
#include <cassert>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

enum class JSONType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class JSONValue;
using JSONArray = std::vector<JSONValue>;

class JSONObject {
public:
    using Storage = std::map<std::string, JSONValue, std::less<>>;
private:
    Storage values_;
public:
    JSONValue& operator[](std::string_view key);
    const JSONValue& at(std::string_view key) const noexcept;

    bool contains(std::string_view key) const noexcept;

    std::size_t size() const noexcept;
    bool empty() const noexcept;

    void erase(std::string_view key);

    Storage::iterator begin() noexcept;
    Storage::iterator end() noexcept;

    Storage::const_iterator begin() const noexcept;
    Storage::const_iterator end() const noexcept;
};

class JSONValue {
public:
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, JSONArray, JSONObject>;
private:
    Storage value_;

    template<typename T, typename U>
    static std::optional<T> tryConvert(const U& value) {
        if constexpr (requires { static_cast<T>(value); })
            return static_cast<T>(value);
        else
            return std::nullopt;
    }

    template<typename T>
    std::optional<T> tryConvertStored() const {
        return std::visit([](const auto& value) -> std::optional<T> {
            using U = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, U>) {
                return value;
            } else if constexpr (requires { static_cast<T>(value); }) {
                return static_cast<T>(value);
            } else {
                return std::nullopt;
            }

        }, this->value_);
    }

public:
    JSONValue() : value_(nullptr) {}
    JSONValue(std::nullptr_t) : value_(nullptr) {}
    JSONValue(bool value) : value_(value) {}
    JSONValue(int value) : value_(static_cast<double>(value)) {}
    JSONValue(double value) : value_(value) {}
    JSONValue(const char* value) : value_(std::string(value)) {}
    JSONValue(std::string value) : value_(std::move(value)) {}
    JSONValue(JSONArray value) : value_(std::move(value)) {}
    JSONValue(JSONObject value) : value_(std::move(value)) {}

    static const JSONValue& null() noexcept {
        static const JSONValue value;
        return value;
    }

    JSONType getType() const noexcept {
        return static_cast<JSONType>(this->value_.index());
    }

    bool isNull() const noexcept {
        return this->is<std::nullptr_t>();
    }

    template<typename T>
    bool is() const noexcept {
        return std::holds_alternative<T>(this->value_);
    }

    template<typename T>
    bool isConvertible() const noexcept {
        return this->tryConvertStored<T>().has_value();
    }

    template<typename T>
    T* tryGet() noexcept {
        if constexpr (std::disjunction_v<
            std::is_same<T, std::nullptr_t>,
            std::is_same<T, bool>,
            std::is_same<T, double>,
            std::is_same<T, std::string>,
            std::is_same<T, JSONArray>,
            std::is_same<T, JSONObject>>) {
            return std::get_if<T>(&this->value_);
        } else {
            return nullptr;
        }
    }

    template<typename T>
    const T* tryGet() const noexcept {
        if constexpr (std::disjunction_v<
            std::is_same<T, std::nullptr_t>,
            std::is_same<T, bool>,
            std::is_same<T, double>,
            std::is_same<T, std::string>,
            std::is_same<T, JSONArray>,
            std::is_same<T, JSONObject>>) {
            return std::get_if<T>(&this->value_);
        } else {
            return nullptr;
        }
    }

    template<typename T>
    T& get() & noexcept {
        assert(this->is<T>() && "Type mismatch");
        return *std::get_if<T>(&this->value_);
    }

    template<typename T>
    const T& get() const& noexcept {
        assert(this->is<T>() && "Type mismatch");
        return *std::get_if<T>(&this->value_);
    }

    template<typename T>
    T&& get() && noexcept {
        assert(this->is<T>() && "Type mismatch");
        return std::move(*std::get_if<T>(&this->value_));
    }

    template<typename T>
    T getOrDefault(const T& defaultValue = T{}) const {
        if (const T* value = this->tryGet<T>())
            return *value;

        if (const std::optional<T> value = this->tryConvertStored<T>())
            return *value;

        return defaultValue;
    }

    std::string getOrDefault(std::string_view defaultValue) const {
        if (const std::string* value = this->tryGet<std::string>())
            return *value;

        return std::string(defaultValue);
    }

    bool getOrDefault(bool defaultValue = false) const noexcept {
        if (const bool* value = this->tryGet<bool>())
            return *value;

        if (const double* value = this->tryGet<double>())
            return static_cast<bool>(*value);

        return defaultValue;
    }

    double getOrDefault(double defaultValue = 0.0) const noexcept {
        if (const double* value = this->tryGet<double>())
            return *value;

        if (const bool* value = this->tryGet<bool>())
            return static_cast<double>(*value);

        return defaultValue;
    }

    int getOrDefault(int defaultValue = 0) const noexcept {
        if (const double* value = this->tryGet<double>())
            return static_cast<int>(*value);

        if (const bool* value = this->tryGet<bool>())
            return static_cast<int>(*value);

        return defaultValue;
    }

    const JSONValue& at(std::string_view key) const noexcept {
        const JSONObject* object = this->tryGet<JSONObject>();
        return object ? object->at(key) : null();
    }

    const JSONValue& at(std::size_t index) const noexcept {
        const JSONArray* array = this->tryGet<JSONArray>();
        return array && index < array->size() ? (*array)[index] : null();
    }

    JSONValue& operator[](std::string_view key) {
        return this->get<JSONObject>()[key];
    }

    JSONValue& operator[](std::size_t index) {
        JSONArray& array = this->get<JSONArray>();

        assert(index < array.size() && "Out-of-bounds");
        return array[index];
    }

    Storage& storage() noexcept {
        return this->value_;
    }

    const Storage& storage() const noexcept {
        return this->value_;
    }
};

inline JSONValue& JSONObject::operator[](std::string_view key) {
    auto iterator = this->values_.lower_bound(key);
    if (iterator == this->values_.end() || iterator->first != key)
        iterator = this->values_.emplace_hint(iterator, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());

    return iterator->second;
}

inline const JSONValue& JSONObject::at(std::string_view key) const noexcept {
    auto iterator = this->values_.find(key);
    return iterator != this->values_.end() ? iterator->second : JSONValue::null();
}

inline bool JSONObject::contains(std::string_view key) const noexcept {
    return this->values_.find(key) != this->values_.end();
}

inline std::size_t JSONObject::size() const noexcept {
    return this->values_.size();
}

inline bool JSONObject::empty() const noexcept {
    return this->values_.empty();
}

inline void JSONObject::erase(std::string_view key) {
    auto iterator = this->values_.find(key);
    if (iterator != this->values_.end())
        this->values_.erase(iterator);
}

inline JSONObject::Storage::iterator JSONObject::begin() noexcept {
    return this->values_.begin();
}

inline JSONObject::Storage::iterator JSONObject::end() noexcept {
    return this->values_.end();
}

inline JSONObject::Storage::const_iterator JSONObject::begin() const noexcept {
    return this->values_.begin();
}

inline JSONObject::Storage::const_iterator JSONObject::end() const noexcept {
    return this->values_.end();
}
