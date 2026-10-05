#pragma once
#include "BufferResource.hpp"

#include <rend/Common/RefCounter.hpp>

#include <cstdint>

namespace rend {
	class ConstantBuffer : public rend::RefCounter<rend::BufferResource> {
	public:
		[[nodiscard]] bool init(uint32_t dataSize, const void* initialData = nullptr);
	};

	typedef rend::RefCountPtr<rend::ConstantBuffer> ConstantBufferHandle;
}