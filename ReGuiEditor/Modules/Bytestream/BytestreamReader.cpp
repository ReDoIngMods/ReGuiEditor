#include "BytestreamReader.hpp"

#include <LZ4/lz4.h>

#include <cstdint>
#include <cstring>
#include <vector>

ReGuiEditor::BytestreamReader::BytestreamReader(const void* data, size_t size) {
	if (data == nullptr || size == 0) {
		this->begin_ = nullptr;
		this->end_ = nullptr;
		this->position_ = nullptr;

		return;
	}

	this->begin_ = new char[size];
	memcpy(this->begin_, data, size);
	
	this->end_ = this->begin_ + size;
	this->position_ = this->begin_;
}

ReGuiEditor::BytestreamReader::BytestreamReader(BytestreamReader& other) {
	this->begin_ = new char[other.size()];
	memcpy(this->begin_, other.begin_, other.size());
	this->end_ = this->begin_ + other.size();
	this->position_ = this->begin_ + (other.position_ - other.begin_);
}

ReGuiEditor::BytestreamReader::BytestreamReader(BytestreamReader&& other) noexcept {
	this->begin_ = other.begin_;
	this->end_ = other.end_;
	this->position_ = other.position_;

	other.begin_ = nullptr;
	other.end_ = nullptr;
	other.position_ = nullptr;
}

ReGuiEditor::BytestreamReader& ReGuiEditor::BytestreamReader::operator=(BytestreamReader& other) {
	if (this == &other)
		return *this;

	delete[] this->begin_;

	this->begin_ = new char[other.size()];
	memcpy(this->begin_, other.begin_, other.size());

	this->end_ = this->begin_ + other.size();
	this->position_ = this->begin_ + (other.position_ - other.begin_);

	return *this;
}

ReGuiEditor::BytestreamReader& ReGuiEditor::BytestreamReader::operator=(BytestreamReader&& other) noexcept {
	if (this == &other)
		return *this;

	delete[] this->begin_;
	this->begin_ = other.begin_;
	this->end_ = other.end_;
	this->position_ = other.position_;

	other.begin_ = nullptr;
	other.end_ = nullptr;
	other.position_ = nullptr;
	return *this;
}

ReGuiEditor::BytestreamReader::~BytestreamReader() {
	delete[] this->begin_;
}

bool ReGuiEditor::BytestreamReader::readBytes(char* output, size_t length) {
	if (length > this->remaining())
		return false;

	memcpy(output, this->position_, length);
	this->position_ += length;
	return true;
}

bool ReGuiEditor::BytestreamReader::verifyBytes(const char* expectedData, size_t length) {
	if (length > this->remaining())
		return false;
	
	if (memcmp(this->position_, expectedData, length) != 0)
		return false;

	this->position_ += length;
	return true;
}

bool ReGuiEditor::BytestreamReader::verifyBytes(const char* expectedData) {
	if (expectedData == nullptr)
		return false;

	return this->verifyBytes(expectedData, strlen(expectedData));
}

bool ReGuiEditor::BytestreamReader::readCompressableData(std::vector<char>& output) {
	if (this->remaining() < sizeof(uint32_t) * 2)
		return false;

	const uint32_t uncompressedSize = this->read<uint32_t>();
	const uint32_t compressedSize = this->read<uint32_t>();

	if (compressedSize == 0) {
		if (this->remaining() < uncompressedSize)
			return false;

		output.resize(uncompressedSize);
		return this->readBytes(output.data(), uncompressedSize);
	}

	if (uncompressedSize == 0 || this->remaining() < compressedSize)
		return false;

	output.resize(uncompressedSize);

	int decompressedSize = LZ4_decompress_safe(this->position_, output.data(), static_cast<int>(compressedSize), static_cast<int>(uncompressedSize));
	if (decompressedSize < 0 || static_cast<size_t>(decompressedSize) != uncompressedSize) {
		output.clear();
		return false;
	}

	this->position_ += compressedSize;
	return true;
}