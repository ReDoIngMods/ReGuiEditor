#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace ReGuiEditor {
	class BytestreamReader {
		char* begin_ = nullptr;
		char* end_ = nullptr;
		char* position_ = nullptr;
	public:
		BytestreamReader() = default;
		~BytestreamReader();
		
		explicit BytestreamReader(const void* data, size_t size);

		BytestreamReader(BytestreamReader&);
		BytestreamReader(BytestreamReader&&) noexcept;
		BytestreamReader& operator=(BytestreamReader&);
		BytestreamReader& operator=(BytestreamReader&&) noexcept;

		bool readBytes(char* output, size_t length);
		
		template<typename T>
		T read() {
			static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

			T output{};
			(void)this->readBytes(reinterpret_cast<char*>(&output), sizeof(T));

			return output;
		}

		template<>
		std::string read<std::string>() {
			const uint32_t length = this->read<uint32_t>();
			if (length > this->remaining())
				return {};

			std::string output(length, '\0');
			(void)this->readBytes(output.data(), length);
			return output;
		}

		bool verifyBytes(const char* expectedData, size_t length);
		bool verifyBytes(const char* expectedData);

		template<typename T>
		bool verify(T expectedValue) {
			if (this->remaining() < sizeof(T))
				return false;

			return this->read<T>() == expectedValue;
		}

		template<>
		bool verify<std::string>(std::string expectedValue) {
			const uint32_t length = this->read<uint32_t>();
			if (this->remaining() < length)
				return false;

			std::string actualValue(length, '\0');
			(void)this->readBytes(actualValue.data(), length);

			return actualValue == expectedValue;
		}

		char* data() const { return this->begin_; }
		size_t size() const { return static_cast<size_t>(this->end_ - this->begin_); }
		size_t remaining() const { return static_cast<size_t>(this->end_ - this->position_); }

		char* begin() const { return this->begin_; }
		char* cbegin() const { return this->begin_; }
		char* rbegin() const { return this->end_ - 1; }

		char* end() const { return this->end_; }
		char* cend() const { return this->end_; }
		char* rend() const { return this->begin_ - 1; }

		bool readCompressableData(std::vector<char>& output);

		template<typename T>
		inline bool readCompressableData(std::vector<T>& output) {
			std::vector<char> data;
			if (!this->readCompressableData(data))
				return false;

			if (data.size() % sizeof(T) != 0)
				return false;
			
			const size_t elementCount = data.size() / sizeof(T);
			output.resize(elementCount);
			
			memcpy(output.data(), data.data(), data.size());
			return true;
		}
	};
}