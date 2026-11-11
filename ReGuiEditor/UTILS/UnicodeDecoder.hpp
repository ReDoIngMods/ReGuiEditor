#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace UTILS {
	class UnicodeDecoder {
	public:
		UnicodeDecoder() = delete;

		static std::optional<std::wstring> fromUTF8(std::span<const uint8_t> data);
		static std::optional<std::wstring> fromUTF16(std::span<const uint8_t> data, bool littleEndian);
		static std::optional<std::wstring> fromUTF32(std::span<const uint8_t> data, bool littleEndian);
	};
}