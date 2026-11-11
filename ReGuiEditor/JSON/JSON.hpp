#pragma once
#include <cstdint>
#include <istream>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

enum class JSONType : uint8_t {
	None,
	Null,
	Boolean,
	FloatingPoint,
	Integer,
	String,
	Object,
	Array
};

class JSONValue;
class JSONObject;
using JSONArray = std::vector<JSONValue>;

namespace std {
	template<> struct hash<JSONValue>;
	template<> struct hash<JSONObject>;
	template<> struct hash<JSONArray>;
}

class JSONObject {
public:
	struct Impl;
private:
	std::unique_ptr<Impl> impl_;
public:
	JSONObject();
	~JSONObject();

	JSONObject(const JSONObject& other);
	JSONObject(JSONObject&& other) noexcept;

	JSONObject& operator=(const JSONObject& other);
	JSONObject& operator=(JSONObject&& other) noexcept;

	class iterator;
	class const_iterator;

	[[nodiscard]] iterator begin();
	[[nodiscard]] iterator end();

	[[nodiscard]] const_iterator begin() const;
	[[nodiscard]] const_iterator end()   const;

	[[nodiscard]] const_iterator cbegin() const;
	[[nodiscard]] const_iterator cend()   const;

	[[nodiscard]] const_iterator find(const std::string& key) const;
	[[nodiscard]] iterator       find(const std::string& key);

	[[nodiscard]] const JSONValue& operator[](const std::string& key) const;
	[[nodiscard]] JSONValue& operator[](const std::string& key);

	[[nodiscard]] const JSONValue& at(const std::string& key) const;
	[[nodiscard]] JSONValue& at(const std::string& key);

	[[nodiscard]] bool contains(const std::string& key) const;

	template<typename... Args>
	void emplace(const std::string& key, Args&&... args);

	void insert(const std::string& key, const JSONValue& value);
	void insert(const std::string& key, JSONValue&& value);

	void erase(const std::string& key);

	[[nodiscard]] std::size_t size()  const;
	[[nodiscard]] bool        empty() const;

	[[nodiscard]] const std::unordered_map<std::string, JSONValue>& getValues() const;

	[[nodiscard]] bool operator==(const JSONObject& other) const;
	[[nodiscard]] bool operator!=(const JSONObject& other) const;
};

class JSONParserResult {
	int line_ = 0;
	int column_ = 0;
	int position_ = 0;
	std::string message_ = "No error";
	bool success_ = true;

	JSONParserResult(int line, int column, int position, const std::string& message) : line_(line), column_(column), position_(position), message_(message), success_(false) {}
public:
	JSONParserResult() = default;

	static JSONParserResult success() { return JSONParserResult(); }
	static JSONParserResult error(int line, int column, int position, const std::string& message) { return JSONParserResult(line, column, position, message); }

	explicit operator bool() const { return this->success_; }

	[[nodiscard]] bool isSuccess() const { return this->success_; }
	[[nodiscard]] bool isError()   const { return !this->success_; }

	[[nodiscard]] int getLine()     const { return this->line_; }
	[[nodiscard]] int getColumn()   const { return this->column_; }
	[[nodiscard]] int getPosition() const { return this->position_; }

	[[nodiscard]] const std::string& getMessage() const { return this->message_; }

	[[nodiscard]] std::string createErrorMessage() const {
		return "error at line " + std::to_string(this->line_) + ", column " + std::to_string(this->column_) + ": " + this->message_;
	}
};

class JSONValue {
public:
	using variant_type = std::variant<std::nullptr_t, bool, int, double, std::string, JSONObject, JSONArray>;
private:
	JSONType type_;
	std::variant<std::nullptr_t, bool, int, double, std::string, JSONObject, JSONArray> value_;
public:

	JSONValue() : type_(JSONType::None), value_(nullptr) {}
	JSONValue(std::nullptr_t) : type_(JSONType::Null), value_(nullptr) {}

	JSONValue(bool   value) : type_(JSONType::Boolean), value_(value) {}
	JSONValue(int    value) : type_(JSONType::Integer), value_(value) {}
	JSONValue(double value) : type_(JSONType::FloatingPoint), value_(value) {}

	JSONValue(const std::string& value) : type_(JSONType::String), value_(value) {}
	JSONValue(std::string&& value) : type_(JSONType::String), value_(std::move(value)) {}

	JSONValue(const JSONObject& value) : type_(JSONType::Object), value_(value) {}
	JSONValue(JSONObject&& value) : type_(JSONType::Object), value_(std::move(value)) {}

	JSONValue(const JSONArray& value) : type_(JSONType::Array), value_(value) {}
	JSONValue(JSONArray&& value) : type_(JSONType::Array), value_(std::move(value)) {}

	[[nodiscard]] bool operator==(const JSONValue& other) const { return this->type_ == other.type_ && this->value_ == other.value_; }
	[[nodiscard]] bool operator!=(const JSONValue& other) const { return !(*this == other); }

	[[nodiscard]] JSONType getType() const { return this->type_; }

	[[nodiscard]] bool isNull()          const { return this->type_ == JSONType::Null; }
	[[nodiscard]] bool isBoolean()       const { return this->type_ == JSONType::Boolean; }
	[[nodiscard]] bool isInteger()       const { return this->type_ == JSONType::Integer; }
	[[nodiscard]] bool isFloatingPoint() const { return this->type_ == JSONType::FloatingPoint; }
	[[nodiscard]] bool isString()        const { return this->type_ == JSONType::String; }
	[[nodiscard]] bool isObject()        const { return this->type_ == JSONType::Object; }
	[[nodiscard]] bool isArray()         const { return this->type_ == JSONType::Array; }
	[[nodiscard]] bool isNone()          const { return this->type_ == JSONType::None; }

	[[nodiscard]] bool isNumeric() const { return this->type_ == JSONType::Integer || this->type_ == JSONType::FloatingPoint; }

	[[nodiscard]] std::optional<bool>        getBoolean() const { if (this->isBoolean()) return std::get<bool       >(this->value_); return std::nullopt; }
	[[nodiscard]] std::optional<int>         getInteger() const { if (this->isInteger()) return std::get<int>(this->value_); if (this->isFloatingPoint()) return static_cast<int>(std::get<double>(this->value_)); return std::nullopt; }
	[[nodiscard]] std::optional<double>      getDouble()  const { if (this->isFloatingPoint()) return std::get<double>(this->value_); if (this->isInteger()) return static_cast<double>(std::get<int>(this->value_)); return std::nullopt; }
	[[nodiscard]] std::optional<std::string> getString()  const { if (this->isString()) return std::get<std::string>(this->value_); return std::nullopt; }
	[[nodiscard]] std::optional<JSONObject>  getObject()  const { if (this->isObject()) return std::get<JSONObject >(this->value_); return std::nullopt; }
	[[nodiscard]] std::optional<JSONArray>   getArray()   const { if (this->isArray()) return std::get<JSONArray  >(this->value_); return std::nullopt; }

	[[nodiscard]] bool   asBoolean() const { return std::get<bool       >(this->value_); }
	[[nodiscard]] int    asInteger() const { return std::get<int        >(this->value_); }
	[[nodiscard]] double asDouble() const { return std::get<double     >(this->value_); }
	[[nodiscard]] const std::string& asString() const { return std::get<std::string>(this->value_); }

	[[nodiscard]] const JSONObject& asObject() const { return std::get<JSONObject >(this->value_); }
	[[nodiscard]] const JSONArray& asArray()  const { return std::get<JSONArray  >(this->value_); }
	[[nodiscard]] JSONObject& asObject() { return std::get<JSONObject >(this->value_); }
	[[nodiscard]] JSONArray& asArray() { return std::get<JSONArray  >(this->value_); }

	void        dump(std::string& str, bool prettyPrint = false, const char indentCharacter = ' ') const;
	void        dump(std::ostream& stream, bool prettyPrint = false, const char indentCharacter = ' ') const;
	[[nodiscard]] std::string dump(bool prettyPrint = false, const char indentCharacter = ' ') const;

	[[nodiscard]] static JSONParserResult parse(std::istream& stream, JSONValue& output);
	[[nodiscard]] static JSONParserResult parse(const std::string& jsonString, JSONValue& output);
};

struct JSONObject::Impl {
	std::unordered_map<std::string, JSONValue> values_;
};

class JSONObject::iterator {
public:
	using iterator_category = std::bidirectional_iterator_tag;
	using value_type = std::pair<const std::string, JSONValue>;
	using difference_type = std::ptrdiff_t;
	using pointer = std::pair<const std::string, JSONValue>*;
	using reference = std::pair<const std::string, JSONValue>&;

	std::unordered_map<std::string, JSONValue>::iterator iterator_;
public:
	iterator(std::unordered_map<std::string, JSONValue>::iterator iterator) : iterator_(iterator) {}

	[[nodiscard]] reference operator*() { return   *this->iterator_; }
	[[nodiscard]] pointer   operator->() { return &(*this->iterator_); }

	[[nodiscard]] bool operator==(const iterator& other) const { return this->iterator_ == other.iterator_; }
	[[nodiscard]] bool operator!=(const iterator& other) const { return !(*this == other); }

	iterator& operator++() { ++this->iterator_; return *this; }
	iterator& operator--() { --this->iterator_; return *this; }
};

class JSONObject::const_iterator {
public:
	using iterator_category = std::bidirectional_iterator_tag;
	using value_type = std::pair<const std::string, JSONValue>;
	using difference_type = std::ptrdiff_t;
	using pointer = const std::pair<const std::string, JSONValue>*;
	using reference = const std::pair<const std::string, JSONValue>&;

	std::unordered_map<std::string, JSONValue>::const_iterator iterator_;
public:
	const_iterator(std::unordered_map<std::string, JSONValue>::const_iterator iterator) : iterator_(iterator) {}

	[[nodiscard]] reference operator*()  const { return   *this->iterator_; }
	[[nodiscard]] pointer   operator->() const { return &(*this->iterator_); }

	[[nodiscard]] bool operator==(const const_iterator& other) const { return this->iterator_ == other.iterator_; }
	[[nodiscard]] bool operator!=(const const_iterator& other) const { return !(*this == other); }

	const_iterator& operator++() { ++this->iterator_; return *this; }
	const_iterator& operator--() { --this->iterator_; return *this; }
};

inline JSONObject::JSONObject() : impl_(std::make_unique<Impl>()) {}
inline JSONObject::~JSONObject() = default;

inline JSONObject::JSONObject(const JSONObject& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}
inline JSONObject::JSONObject(JSONObject&& other) noexcept = default;

inline JSONObject& JSONObject::operator=(const JSONObject& other) {
	if (this != &other) this->impl_ = std::make_unique<Impl>(*other.impl_);
	return *this;
}
inline JSONObject& JSONObject::operator=(JSONObject&& other) noexcept = default;

inline JSONObject::iterator JSONObject::begin() { return iterator(this->impl_->values_.begin()); }
inline JSONObject::iterator JSONObject::end() { return iterator(this->impl_->values_.end()); }

inline JSONObject::const_iterator JSONObject::begin() const { return const_iterator(this->impl_->values_.begin()); }
inline JSONObject::const_iterator JSONObject::end()   const { return const_iterator(this->impl_->values_.end()); }

inline JSONObject::const_iterator JSONObject::cbegin() const { return this->begin(); }
inline JSONObject::const_iterator JSONObject::cend()   const { return this->end(); }

inline JSONObject::const_iterator JSONObject::find(const std::string& key) const { return const_iterator(this->impl_->values_.find(key)); }
inline JSONObject::iterator       JSONObject::find(const std::string& key) { return       iterator(this->impl_->values_.find(key)); }

inline const JSONValue& JSONObject::operator[](const std::string& key) const { return this->impl_->values_.at(key); }
inline       JSONValue& JSONObject::operator[](const std::string& key) { return this->impl_->values_[key]; }

inline const JSONValue& JSONObject::at(const std::string& key) const { return this->impl_->values_.at(key); }
inline       JSONValue& JSONObject::at(const std::string& key) { return this->impl_->values_.at(key); }

inline bool JSONObject::contains(const std::string& key) const { return this->impl_->values_.contains(key); }

template<typename... Args>
inline void JSONObject::emplace(const std::string& key, Args&&... args) {
	this->impl_->values_.emplace(key, std::forward<Args>(args)...);
}

inline void JSONObject::insert(const std::string& key, const JSONValue& value) { this->impl_->values_.insert({ key,           value }); }
inline void JSONObject::insert(const std::string& key, JSONValue&& value) { this->impl_->values_.insert({ key, std::move(value) }); }

inline void JSONObject::erase(const std::string& key) { this->impl_->values_.erase(key); }
inline std::size_t JSONObject::size() const { return this->impl_->values_.size(); }
inline bool JSONObject::empty() const { return this->impl_->values_.empty(); }

inline const std::unordered_map<std::string, JSONValue>& JSONObject::getValues() const { return this->impl_->values_; }

inline bool JSONObject::operator==(const JSONObject& other) const { return this->impl_->values_ == other.impl_->values_; }
inline bool JSONObject::operator!=(const JSONObject& other) const { return !(*this == other); }

namespace std {
	template<>
	struct hash<JSONType> {
		[[nodiscard]] std::size_t operator()(const JSONType& type) const {
			return std::hash<std::underlying_type_t<JSONType>>()(static_cast<std::underlying_type_t<JSONType>>(type));
		}
	};

	template<> struct hash<JSONValue > { [[nodiscard]] std::size_t operator()(const JSONValue& value) const; };
	template<> struct hash<JSONObject> { [[nodiscard]] std::size_t operator()(const JSONObject& object) const; };
	template<> struct hash<JSONArray > { [[nodiscard]] std::size_t operator()(const JSONArray& array) const; };

	inline std::size_t hash<JSONValue>::operator()(const JSONValue& value) const {
		std::size_t hashValue = std::hash<JSONType>()(value.getType());

		switch (value.getType()) {
		case JSONType::None:
			break;
		case JSONType::Null:
			hashValue ^= 0;
			break;
		case JSONType::Boolean:       hashValue ^= std::hash<bool       >()(value.getBoolean().value()); break;
		case JSONType::FloatingPoint: hashValue ^= std::hash<double     >()(value.getDouble().value()); break;
		case JSONType::Integer:       hashValue ^= std::hash<int        >()(value.getInteger().value()); break;
		case JSONType::String:        hashValue ^= std::hash<std::string>()(value.getString().value()); break;
		case JSONType::Object:        hashValue ^= std::hash<JSONObject >()(value.getObject().value()); break;
		case JSONType::Array:         hashValue ^= std::hash<JSONArray  >()(value.getArray().value()); break;
		}

		return hashValue;
	}

	inline std::size_t hash<JSONObject>::operator()(const JSONObject& object) const {
		std::size_t hashValue = 0;
		for (const auto& [key, value] : object) {
			std::size_t keyHash = std::hash<std::string>()(key);
			std::size_t valueHash = std::hash<JSONValue>()(value);
			hashValue ^= keyHash ^ valueHash;
		}

		return hashValue;
	}

	inline std::size_t hash<JSONArray>::operator()(const JSONArray& array) const {
		std::size_t hashValue = 0;
		for (const auto& value : array) {
			std::size_t valueHash = std::hash<JSONValue>()(value);
			hashValue ^= valueHash;
		}

		return hashValue;
	}
}