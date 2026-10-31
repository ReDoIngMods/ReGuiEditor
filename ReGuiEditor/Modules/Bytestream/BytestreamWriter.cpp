#include "BytestreamWriter.hpp"

#include <LZ4/lz4.h>

#include <cstdint>
#include <cstring>
#include <memory>

constexpr size_t COMPRESS_AFTER_SIZE_THRESHOLD = 4096;

inline static size_t calculateNewCapacity(size_t requiredSize) {
	size_t newCapacity = 1;
	while (newCapacity < requiredSize)
		newCapacity *= 2;

	return newCapacity;
}

void ReGuiEditor::BytestreamWriter::ensureCapacity(size_t additionalSize) {
	size_t requiredSize = this->position_ + additionalSize;
	if (requiredSize > this->capacity_) {
		size_t newCapacity = calculateNewCapacity(requiredSize);

		char* newData = new char[newCapacity];
		if (this->data_) {
			memcpy(newData, this->data_, this->position_);
			delete[] this->data_;
		}

		this->data_ = newData;
		this->capacity_ = newCapacity;
	}
}

ReGuiEditor::BytestreamWriter::BytestreamWriter() {
	this->capacity_ = 16;
	this->data_ = new char[this->capacity_];
}

ReGuiEditor::BytestreamWriter::~BytestreamWriter() {
	delete[] this->data_;
}

void ReGuiEditor::BytestreamWriter::writeBytes(const char* data, size_t length) {
	if (length == 0)
		return;

	this->ensureCapacity(length);

	memcpy(this->data_ + this->position_, data, length);
	this->position_ += length;
}

void ReGuiEditor::BytestreamWriter::writeCompressableData(const char* data, size_t length) {
	if (length == 0) {
		this->write<uint32_t>(0);
		this->write<uint32_t>(0);
		return;
	}

	this->write<uint32_t>(static_cast<uint32_t>(length));

	if (length < COMPRESS_AFTER_SIZE_THRESHOLD) {
		this->write<uint32_t>(0);
		this->writeBytes(data, length);
		return;
	}

	const int maxCompressedSize = LZ4_compressBound(static_cast<int>(length));
	if (maxCompressedSize <= 0) {
		this->write<uint32_t>(0);
		this->writeBytes(data, length);
		return;
	}

	const std::unique_ptr<char[]> compressedData = std::make_unique<char[]>(static_cast<size_t>(maxCompressedSize));
	const int compressedSize = LZ4_compress_default(data, compressedData.get(), static_cast<int>(length), maxCompressedSize);
	if (compressedSize <= 0) {
		this->write<uint32_t>(0);
		this->writeBytes(data, length);
		return;
	}

	this->write<uint32_t>(static_cast<uint32_t>(compressedSize));
	this->writeBytes(compressedData.get(), static_cast<size_t>(compressedSize));
}
