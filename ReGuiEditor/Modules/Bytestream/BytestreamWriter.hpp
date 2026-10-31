#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace ReGuiEditor {
	class BytestreamWriter {
		char* data_ = nullptr;
		size_t position_ = 0;
		size_t capacity_ = 0;

		void ensureCapacity(size_t additionalSize);
	public:
		BytestreamWriter();
		~BytestreamWriter();

		void writeBytes(const char* data, size_t length);

		template<typename T>
		void write(const T& value) {
			static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

			this->ensureCapacity(sizeof(T));
			memcpy(this->data_ + this->position_, &value, sizeof(T));
			this->position_ += sizeof(T);
		}

		template<>
		void write<std::string>(const std::string& value) {
			const uint32_t length = static_cast<uint32_t>(value.size());
			this->write(length);
			this->writeBytes(value.data(), length);
		}

		const char* data() const { return this->data_; }
		size_t position() const { return this->position_; }
		size_t size() const { return this->position_; }
		size_t capacity() const { return this->capacity_; }

		char* begin() { return this->data_; }
		char* cbegin() const { return this->data_; }
		char* rbegin() { return this->data_ + this->position_; }

		char* end() { return this->data_ + this->position_; }
		char* cend() const { return this->data_ + this->position_; }
		char* rend() { return this->data_; }

		void writeCompressableData(const char* data, size_t length);
		
		template<typename T>
		inline void writeCompressableData(const T* data, size_t length) {
			static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
			static_assert(std::is_standard_layout_v<T>, "T must be standard layout");
		
			const size_t dataSize = length * sizeof(T);
			this->writeCompressableData(reinterpret_cast<const char*>(data), dataSize);
		}

		inline void writeCompressableData(const std::vector<char>& data) {
			this->writeCompressableData(data.data(), data.size());
		}

		template<typename T>
		inline void writeCompressableData(const std::vector<T>& vec) {
			this->writeCompressableData(vec.data(), vec.size());
		}
	};
}