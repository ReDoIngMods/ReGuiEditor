#pragma once
#include <UTILS/Singleton.hpp>

#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>

class ConfigManager : public UTILS::Singleton<ConfigManager> {
	using ConfigValue = std::variant<bool, int, float, std::string>;

	bool hasChanges_ = false;
	std::unordered_map<std::string, ConfigValue> data_;
	std::unordered_map<std::string, ConfigValue> defaultData_;
public:
	ConfigManager();
	~ConfigManager();

	bool loadConfig();
	bool saveConfig();

	template<typename T>
	void createConfig(const std::string& key, const T& value) {
		auto [iterator, inserted] = this->data_.emplace(key, value);
		if (inserted) {
			iterator->second = value;
			this->hasChanges_ = true;
		}
		
		this->defaultData_[key] = value;
	}

	template<typename T>
	bool getConfigVariable(const std::string& key, T& outputValue) {
		auto iterator = this->data_.find(key);
		if (iterator == this->data_.end())
			return false;

		if (std::holds_alternative<T>(iterator->second)) {
			outputValue = std::get<T>(iterator->second);
			return true;
		}

		return false;
	}

	template<typename T>
	T getConfigVariableWithDefault(const std::string& key, const T defaultValue) {
		auto iterator = this->data_.find(key);
		if (iterator != this->data_.end()) {
			if (std::holds_alternative<T>(iterator->second))
				return std::get<T>(iterator->second);
		}

		return defaultValue;
	}

	template<typename T>
	bool setConfigVariable(const std::string& key, const T& value) {
		auto iterator = this->data_.find(key);
		if (iterator == this->data_.end())
			return false;

		if (std::holds_alternative<T>(iterator->second)) {
			if (iterator->second == value) {
				iterator->second = value;
				this->hasChanges_ = true;
			}

			return true;
		}
		
		return false;
	}

	void resetConfig() {
		this->data_ = this->defaultData_;
		this->hasChanges_ = true;
	}
};