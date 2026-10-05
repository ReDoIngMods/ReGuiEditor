#pragma once
#include <cstdint>

namespace rend {
	class Unknown {
	protected:
		Unknown() = default;
		virtual ~Unknown() = default;
	public:
		Unknown(const Unknown&) = delete;
		Unknown(Unknown&&) = delete;
		Unknown& operator=(const Unknown&) = delete;
		Unknown& operator=(Unknown&&) = delete;

		virtual uint32_t AddRef() noexcept = 0;
		virtual uint32_t Release() = 0;
		[[nodiscard]] virtual uint32_t GetRefCount() const noexcept = 0;
	};
}
