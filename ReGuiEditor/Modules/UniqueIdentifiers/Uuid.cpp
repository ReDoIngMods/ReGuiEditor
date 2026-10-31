#include "Uuid.hpp"

#include "../Math/Hashing.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>

static thread_local std::random_device randomDevice;
static thread_local std::mt19937_64 randomGenerator(randomDevice());

inline static const char fromHexValue(const char character) {
	if (character >= '0' && character <= '9') return character - '0';
	if (character >= 'a' && character <= 'f') return character - 'a' + 10;
	if (character >= 'A' && character <= 'F') return character - 'A' + 10;
	return -1;
}

bool ReGuiEditor::Uuid::fromString(std::string_view text, ReGuiEditor::Uuid& output) {
	if (text.size() != 36)
		return false;

	ReGuiEditor::Uuid result;
	size_t bytesIndex = 0;
	for (size_t textIndex = 0; textIndex < text.size();) {
		if (text[textIndex] == '-') {
			if (textIndex == 8 || textIndex == 13 || textIndex == 18 || textIndex == 23) {
				++textIndex;
				continue;
			}

			return false;
		}

		const int high = fromHexValue(text[textIndex]);
		const int low = fromHexValue(text[textIndex + 1]);
		if (high < 0 || low < 0)
			return false;

		result.bytes[bytesIndex++] = static_cast<uint8_t>(high << 4 | low);
		textIndex += 2;
	}

	output = result;
	return true;
}

ReGuiEditor::Uuid ReGuiEditor::Uuid::generateRandom() {
	ReGuiEditor::Uuid result;
	for (size_t index = 0; index < result.bytes.size(); ++index)
		result.bytes[index] = static_cast<uint8_t>(randomGenerator() & 0xFF);

	result.bytes[6] = (result.bytes[6] & 0x0F) | 0x40;
	result.bytes[8] = (result.bytes[8] & 0x3F) | 0x80;
	return result;
}


ReGuiEditor::Uuid ReGuiEditor::Uuid::generateNamed(const ReGuiEditor::Uuid& uuidNamespace, std::string_view name) {
	std::string message(reinterpret_cast<const char*>(uuidNamespace.bytes.data()), uuidNamespace.bytes.size());
	message.append(name);

	const std::array<uint8_t, 20> digest = ReGuiEditor::generateSHA1(message);

	ReGuiEditor::Uuid result;
	std::copy_n(digest.begin(), 16, result.bytes.begin());

	result.bytes[6] = (result.bytes[6] & 0x0F) | 0x50;
	result.bytes[8] = (result.bytes[8] & 0x3F) | 0x80;

	return result;
}

ReGuiEditor::Uuid ReGuiEditor::Uuid::generateNamed(std::string_view uuidNamespace, std::string_view name) {
	ReGuiEditor::Uuid namespaceUuid;
	if (!ReGuiEditor::Uuid::fromString(uuidNamespace, namespaceUuid))
		return {};

	return ReGuiEditor::Uuid::generateNamed(namespaceUuid, name);
}