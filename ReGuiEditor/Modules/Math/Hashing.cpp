#include "Hashing.hpp" // VCIC-Excluded

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <Windows.h>
#pragma comment(lib, "bcrypt.lib")

class Hashing {
	BCRYPT_ALG_HANDLE algorithmHandle_ = nullptr;
public:
	Hashing(LPCWSTR algorithmId) {
		if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&this->algorithmHandle_, algorithmId, nullptr, 0)))
			this->algorithmHandle_ = nullptr;
	}

	~Hashing() {
		if (this->algorithmHandle_)
			BCryptCloseAlgorithmProvider(this->algorithmHandle_, 0);
	}

	bool isValid() const {
		return this->algorithmHandle_ != nullptr;
	}

	bool hash(std::span<const uint8_t> input, std::span<uint8_t> output) {
		if (!this->isValid())
			return false;

		return BCRYPT_SUCCESS(BCryptHash(this->algorithmHandle_, nullptr, 0, const_cast<PUCHAR>(input.data()), static_cast<ULONG>(input.size()), output.data(), static_cast<ULONG>(output.size())));
	}
};

std::array<uint8_t, 20> ReGuiEditor::generateSHA1(const std::vector<uint8_t>& message) {
	static Hashing sha1Hasher(BCRYPT_SHA1_ALGORITHM);
	if (!sha1Hasher.isValid())
		return {};

	std::array<uint8_t, 20> digest{};
	if (!sha1Hasher.hash(message, digest))
		digest.fill(0);

	return digest;
}

std::array<uint8_t, 32> ReGuiEditor::generateSHA256(const std::vector<uint8_t>& message) {
	static Hashing sha256Hasher(BCRYPT_SHA256_ALGORITHM);
	if (!sha256Hasher.isValid())
		return {};

	std::array<uint8_t, 32> digest{};
	if (!sha256Hasher.hash(message, digest))
		digest.fill(0);

	return digest;
}