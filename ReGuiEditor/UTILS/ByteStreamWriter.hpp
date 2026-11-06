#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace UTILS {
	class ByteStreamWriter {
		char* data_ = nullptr;
		size_t position_ = 0;
		size_t capacity_ = 0;

		inline size_t calculateNewCapacity(size_t requiredSize) {
			size_t newCapacity = 1;
			while (newCapacity < requiredSize)
				newCapacity *= 2;
			
			return newCapacity;
		}

		void ensureCapacity(size_t additionalSize) {
			size_t requiredSize = this->position_ + additionalSize;
			if (requiredSize > this->capacity_) {
				size_t newCapacity = this->calculateNewCapacity(requiredSize);
				
				char* newData = new char[newCapacity];
				if (this->data_) {
					memcpy(newData, this->data_, this->position_);
					delete[] this->data_;
				}
		
				this->data_ = newData;
				this->capacity_ = newCapacity;
			}
		}
	public:
		explicit ByteStreamWriter() {
			this->capacity_ = this->calculateNewCapacity(16);
			this->data_ = new char[this->capacity_];
		}

		~ByteStreamWriter() {
			delete[] this->data_;
		}

		void writeByte(uint8_t value) {
			this->ensureCapacity(sizeof(uint8_t));
		
			*reinterpret_cast<uint8_t*>(this->data_ + this->position_) = value;
		
			this->position_ += sizeof(uint8_t);
		}

		void writeBytes(const char* data, size_t length) {
			this->ensureCapacity(length);

			memcpy(this->data_ + this->position_, data, length);

			this->position_ += length;
		}

		template<typename T>
		void write(const T& value) {
			static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

			this->ensureCapacity(sizeof(T));
			
			*reinterpret_cast<T*>(this->data_ + this->position_) = value;
		
			this->position_ += sizeof(T);
		}

		void writeString(const std::string& value) {
			uint32_t length = static_cast<uint32_t>(value.size());
			this->write(length);
			this->writeBytes(value.data(), length);
		}

		const char* getData() const {
			return this->data_;
		}

		size_t getSize() const {
			return this->position_;
		}

		std::string toString() const {
			std::string result;
			result.assign(this->data_, this->position_);

			return result;
		}

		std::vector<char> toVector() const {
			std::vector<char> result(this->position_);
			memcpy(result.data(), this->data_, this->position_);

			return result;
		}
	};
}