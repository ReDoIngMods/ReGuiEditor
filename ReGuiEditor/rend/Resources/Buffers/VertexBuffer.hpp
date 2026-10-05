#pragma once
#include "BufferResource.hpp"

#include <rend/Common/RefCounter.hpp>

#include <cstdint>

namespace rend {
	class VertexBuffer : public rend::RefCounter<rend::BufferResource> {
		uint32_t vertexSize_ = 0;
		uint32_t elementCount_ = 0;
	public:
		[[nodiscard]] bool init(uint32_t vertexSize, uint32_t vertexCount, const void* initialData = nullptr, bool allowRead = false, bool allowWrite = true);
		[[nodiscard]] bool resizeBuffer(uint32_t newCount, bool copyOldData = false);

		[[nodiscard]] uint32_t getTotalVertices() const { return this->elementCount_; }
		[[nodiscard]] uint32_t getStride() const { return this->vertexSize_; }
	};

	typedef rend::RefCountPtr<rend::VertexBuffer> VertexBufferHandle;
}