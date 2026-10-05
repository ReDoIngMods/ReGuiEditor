#pragma once
#include <rend/Common/RefCounter.hpp>
#include <rend/Resources/Resource.hpp>

#include <d3d11.h>

#include <cstdint>

namespace rend {
	enum class BufferMapType : uint8_t {
		Read,
		Write,
		ReadWrite,
		WriteDiscard,
		WriteNoOverwrite
	};

	class BufferResource : public rend::Resource {
		bool isMapped_ = false;
	protected:
		bool allowReadAccess_ = false;
		bool allowWriteAccess_ = false;

		rend::RefCountPtr<ID3D11Buffer> buffer_;
		uint32_t bufferSize_ = 0;

		~BufferResource() override;
	public:
		[[nodiscard]] uint32_t getBufferSize() const;

		[[nodiscard]] bool mapBuffer(void** data, rend::BufferMapType mapType);
		[[nodiscard]] bool unmapBuffer();

		template<typename T>
		[[nodiscard]] bool mapBuffer(T** data, rend::BufferMapType mapType) {
			return this->mapBuffer(reinterpret_cast<void**>(data), mapType);
		}

		[[nodiscard]] bool readRawBuffer(void* destination, uint32_t size);
		[[nodiscard]] bool writeRawBuffer(const void* source, uint32_t size);

		[[nodiscard]] rend::RefCountPtr<ID3D11Buffer> getBuffer() const { return this->buffer_; }

		[[nodiscard]] bool isMapped() const { return this->isMapped_; }
		[[nodiscard]] bool isReadAccessAllowed() const { return this->allowReadAccess_; }
		[[nodiscard]] bool isWriteAccessAllowed() const { return this->allowWriteAccess_; }
	};

	typedef rend::RefCountPtr<rend::BufferResource> BufferResourceHandle;
}