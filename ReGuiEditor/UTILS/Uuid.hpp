#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace UTILS {
	struct UUID {
		std::array<uint8_t, 16> bytes{};

		constexpr UUID() = default;
		
		UUID(const char* str) {
			auto uuid = UTILS::UUID::fromString(str);
			if (uuid)
				this->bytes = uuid->bytes;
		}

		bool operator==(const UUID& other) const { return this->bytes == other.bytes; }
		bool operator!=(const UUID& other) const { return !(*this == other); }

		bool isNil() const { return std::all_of(this->bytes.begin(), this->bytes.end(), [](uint8_t b) { return b == 0; }); }

		bool isUuid4() const { return (this->bytes[6] & 0xF0) == 0x40 && (this->bytes[8] & 0xC0) == 0x80; }
		bool isUuid5() const { return (this->bytes[6] & 0xF0) == 0x50 && (this->bytes[8] & 0xC0) == 0x80; }
	
		bool isValid() const { return this->isUuid4() || this->isUuid5(); }

		std::string toAStr() const {
			char buffer[37];
			sprintf_s(buffer, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
				this->bytes[0], this->bytes[1], this->bytes[ 2], this->bytes[ 3], this->bytes[ 4], this->bytes[ 5], this->bytes[ 6], this->bytes[ 7],
				this->bytes[8], this->bytes[9], this->bytes[10], this->bytes[11], this->bytes[12], this->bytes[13], this->bytes[14], this->bytes[15]);

			return buffer;
		}

		std::wstring toWStr() const {
			wchar_t buffer[37];
			swprintf_s(buffer, L"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
				this->bytes[0], this->bytes[1], this->bytes[ 2], this->bytes[ 3], this->bytes[ 4], this->bytes[ 5], this->bytes[ 6], this->bytes[ 7],
				this->bytes[8], this->bytes[9], this->bytes[10], this->bytes[11], this->bytes[12], this->bytes[13], this->bytes[14], this->bytes[15]);
		
			return buffer;
		}

		static std::optional<UUID> fromString(std::string_view text);
		static std::optional<UUID> fromWString(std::wstring_view text);

		static UTILS::UUID generateRandom();
		
		static UTILS::UUID generateNamed(const UTILS::UUID& uuidNamespace, std::string_view name);
		static UTILS::UUID generateNamed(std::string_view uuidNamespace, std::string_view name);
	};
}
