#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

namespace UTILS {
	class ByteStreamReader {
		char* data_ = nullptr;
		size_t size_ = 0;
		size_t position_ = 0;
	public:
		explicit ByteStreamReader(const char* data, size_t size) {
			this->data_ = new char[size];
			memcpy(this->data_, data, size);

			this->size_ = size;
		}

		~ByteStreamReader() {
			delete[] this->data_;
		}
		
		uint8_t readByte() {
			if (this->position_ + sizeof(uint8_t) > this->size_)
				return 0;

			uint8_t output = *reinterpret_cast<const uint8_t*>(this->data_ + this->position_);
			this->position_ += sizeof(uint8_t);
			return output;
		}

		bool readBytes(char* output, size_t length) {
			if (this->position_ + length > this->size_)
				return false;

			memcpy(output, this->data_ + this->position_, length);
			this->position_ += length;
			return true;
		}

		template<typename T>
		T read() {
			static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
			
			if (this->position_ + sizeof(T) > this->size_)
				return T{};

			T output = *reinterpret_cast<const T*>(this->data_ + this->position_);
			this->position_ += sizeof(T);
			return output;
		}

		std::string readString() {
			uint32_t length = this->read<uint32_t>();
			if (length == 0)
				return {};

			if (this->position_ + length > this->size_)
				return {};

			std::string output(this->data_ + this->position_, length);
			this->position_ += length;
			return output;
		}

		size_t getRemaining() const {
			return this->size_ - this->position_;
		}
	};
}