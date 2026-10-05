#pragma once
#include "BufferResource.hpp"

#include <rend/Common/RefCounter.hpp>

#include <cstdint>

namespace rend {
	enum class IndexBufferFormat : uint8_t {
		UInt16 = 57, // DXGI_FORMAT_R16_UINT
		UInt32 = 42  // DXGI_FORMAT_R32_UINT
	};

	inline static uint32_t getIndexBufferFormatSize(IndexBufferFormat format) {
		switch (format) {
		case rend::IndexBufferFormat::UInt16: return sizeof(uint16_t);
		case rend::IndexBufferFormat::UInt32: return sizeof(uint32_t);
		default:
			return 0;
		}
	}

	class IndexBuffer : public rend::RefCounter<rend::BufferResource> {
		rend::IndexBufferFormat format_ = rend::IndexBufferFormat::UInt16;
		uint32_t indexCount_ = 0;
	public:
		[[nodiscard]] bool init(rend::IndexBufferFormat format, uint32_t indexCount, const void* initialData = nullptr, bool allowRead = false, bool allowWrite = true);

		[[nodiscard]] bool resizeBuffer(uint32_t newCount, bool copyOldData = false, bool forceShrink = false);
		[[nodiscard]] bool changeFormat(rend::IndexBufferFormat newFormat, bool copyOldData = false);

		[[nodiscard]] rend::IndexBufferFormat getFormat() const { return this->format_; }
		[[nodiscard]] uint32_t getIndexCount() const { return this->indexCount_; }
	};

	typedef rend::RefCountPtr<rend::IndexBuffer> IndexBufferHandle;
}