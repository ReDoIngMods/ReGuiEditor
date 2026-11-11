#include "UnicodeDecoder.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

inline static uint16_t readU16(std::span<const uint8_t> data, size_t index, bool littleEndian) {
	if (littleEndian)
		return static_cast<uint16_t>(data[index] | (data[index + 1] << 8));
	else
		return static_cast<uint16_t>((data[index] << 8) | data[index + 1]);	
}

inline static uint32_t readU32(std::span<const uint8_t> data, size_t index, bool littleEndian) {
	if (littleEndian)
		return static_cast<uint32_t>(data[index] | (data[index + 1] << 8) | (data[index + 2] << 16) | (data[index + 3] << 24));
	else
		return static_cast<uint32_t>((data[index] << 24) | (data[index + 1] << 16) | (data[index + 2] << 8) | data[index + 3]);
}

inline static void appendCodepoint(std::wstring& str, uint32_t codepoint) {
	if (codepoint <= 0xFFFF) {
		str.push_back(static_cast<wchar_t>(codepoint));
        return;
	}

	codepoint -= 0x10000;

	const wchar_t highSurrogate = static_cast<wchar_t>((codepoint >> 10)   + 0xD800);
	const wchar_t lowSurrogate  = static_cast<wchar_t>((codepoint & 0x3FF) + 0xDC00);

	str.push_back(highSurrogate);
	str.push_back(lowSurrogate);
}

std::optional<std::wstring> UTILS::UnicodeDecoder::fromUTF8(std::span<const uint8_t> data) {
    std::wstring result;
    for (size_t index = 0; index < data.size();) {
        uint32_t codepoint = data[index++];
        uint32_t minValue = 0;
        size_t continuationCount = 0;
    
        if (codepoint <= 0x7F) {
            // ASCII
        } else if ((codepoint & 0xE0) == 0xC0) {
            codepoint &= 0x1F;
    
            minValue = 0x80;
            continuationCount = 1;
        } else if ((codepoint & 0xF0) == 0xE0) {
            codepoint &= 0x0F;
    
            minValue = 0x800;
            continuationCount = 2;
        } else if ((codepoint & 0xF8) == 0xF0) {
            codepoint &= 0x07;
    
            minValue = 0x10000;
            continuationCount = 3;
        } else {
    		// Invalid UTF-8 leading byte
            return std::nullopt;
        }
    
        if (index + continuationCount > data.size())
            return std::nullopt;

		for (size_t continuationIndex = 0; continuationIndex < continuationCount; ++continuationIndex) {
			const uint8_t byte = data[index++];
			if ((byte & 0xC0) != 0x80)
				return std::nullopt;

			codepoint = (codepoint << 6) | (byte & 0x3F);
		}
    
    	if (codepoint < minValue || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
    		return std::nullopt; // Invalid code point
    
        appendCodepoint(result, codepoint);
    }
    
    return result;
}

std::optional<std::wstring> UTILS::UnicodeDecoder::fromUTF16(std::span<const uint8_t> data, bool littleEndian) {
	if (data.size() % 2 != 0)
		return std::nullopt;
	
	std::wstring result;
	for (size_t index = 0; index < data.size(); index += 2) {
		uint32_t codepoint = readU16(data, index, littleEndian);
		if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
			if (index + 3 >= data.size())
				return std::nullopt;
	
			const uint32_t low = readU16(data, index + 2, littleEndian);
			if (low < 0xDC00 || low > 0xDFFF)
				return std::nullopt;
			
			codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
			index += 2;
		} else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
			return std::nullopt;
		}

		appendCodepoint(result, codepoint);
	}

	return result;
}

std::optional<std::wstring> UTILS::UnicodeDecoder::fromUTF32(std::span<const uint8_t> data, bool littleEndian) {
	if (data.size() % 4 != 0)
		return std::nullopt;

	std::wstring result;
	for (size_t index = 0; index < data.size(); index += 4) {
		uint32_t codepoint = readU32(data, index, littleEndian);
		if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
			return std::nullopt;

		appendCodepoint(result, codepoint);
	}

	return result;
}