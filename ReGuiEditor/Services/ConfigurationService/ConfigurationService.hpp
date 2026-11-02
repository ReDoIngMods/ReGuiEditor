#pragma once
#include "../Service.hpp"

#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace ReGuiEditor {
	using ConfigurationValue = std::variant<int, float, std::string, bool>;

	struct ConfigurationProperty {
		std::string key;
		ConfigurationValue value;
	};

	class ConfigurationService : public Service {
		std::vector<ConfigurationProperty> properties_;
		bool isDirty_ = false;

		void saveConfiguration();
	public:
		bool initialize() override;
		
		void tick() override;

		template<typename T>
		bool getProperty(const std::string& key, T& outValue) const {
			for (const auto& property : this->properties_) {
				if (property.key != key)
					continue;

				if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, int>) {
					if (const int* valuePtr = std::get_if<int>(&property.value)) {
						outValue = static_cast<T>(*valuePtr);
						return true;
					}
				} else {
					if (const T* valuePtr = std::get_if<T>(&property.value)) {
						outValue = *valuePtr;
						return true;
					}
				}

				CONSOLE_ERROR("Configuration property '%s' has a different type than requested.", key.c_str());
				return false;
			}

			CONSOLE_WARNING("Configuration property '%s' not found.", key.c_str());
			return false;
		}

		template<typename T>
		T getOrDefault(const std::string& key, const T& defaultValue) {
			T value{};
			if (this->getProperty(key, value))
				return value;

			if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, int>) {
				this->setProperty(key, static_cast<int>(defaultValue));
			} else {
				this->setProperty(key, defaultValue);
			}

			return defaultValue;
		}

		void setProperty(const std::string& key, const ConfigurationValue& value) {
			for (auto& property : this->properties_) {
				if (property.key == key) {
					property.value = value;

					this->isDirty_ = true;
					return;
				}
			}

			this->properties_.emplace_back(key, value);
			this->isDirty_ = true;
		}

		const char* getServiceName() const override { return "ConfigurationService"; }
	};
}