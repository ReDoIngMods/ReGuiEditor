#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace UTILS {
	template<typename T>
	inline void hashCombine(size_t& seed, const T& value) {
		seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	std::array<uint8_t, 20> SHA1(const std::vector<uint8_t>& message);
	std::array<uint8_t, 32> SHA256(const std::vector<uint8_t>& message);

	inline std::array<uint8_t, 20> SHA1(const std::string& message) {
		std::vector<uint8_t> data(message.begin(), message.end());
		return UTILS::SHA1(data);
	}

	inline std::array<uint8_t, 32> SHA256(const std::string& message) {
		std::vector<uint8_t> data(message.begin(), message.end());
		return UTILS::SHA256(data);
	}
}