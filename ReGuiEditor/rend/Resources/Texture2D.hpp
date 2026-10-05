#pragma once
#include "ShaderResourceView.hpp"

#include <rend/Common/Format.hpp>
#include <rend/Common/RefCounter.hpp>

#include <d3d11.h>

#include <cstdint>

namespace rend {
	enum class Texture2DFlags : uint8_t {
		None = 0,
		ShaderResource = 1 << 0,
		RenderTarget = 1 << 1,
		StagingRead = 1 << 2,
		DynamicWrite = 1 << 3
	};

	inline bool operator&(rend::Texture2DFlags a, rend::Texture2DFlags b) {
		return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
	}

	inline rend::Texture2DFlags operator|(rend::Texture2DFlags a, rend::Texture2DFlags b) {
		return static_cast<rend::Texture2DFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
	}

	class Texture2D : public rend::RefCounter<rend::ShaderResourceView> {
		uint32_t width_ = 0;
		uint32_t height_ = 0;
		rend::Format format_ = rend::Format::Unknown;
		rend::Texture2DFlags flags_ = rend::Texture2DFlags::None;

		rend::RefCountPtr<ID3D11Texture2D> texture_;
	public:
		[[nodiscard]] bool init(uint32_t width, uint32_t height, rend::Format format, rend::Texture2DFlags flags);
		[[nodiscard]] bool initFromBackBuffer();

		[[nodiscard]] rend::Texture2DFlags getFlags() const { return this->flags_; }

		[[nodiscard]] bool copyResource(rend::Texture2D* destination) const;

		[[nodiscard]] bool readData(uint8_t* destinationBuffer, uint32_t destinationBufferSize) const;
		[[nodiscard]] bool writeData(const uint8_t* source, uint32_t sourceBufferSize);

		[[nodiscard]] bool readDataRegion(uint8_t* destinationBuffer, size_t destinationBufferSize, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t destinationPitch) const;
		[[nodiscard]] bool writeDataRegion(const uint8_t* sourceBuffer, size_t sourceBufferSize, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t sourcePitch) const;

		[[nodiscard]] uint32_t getWidth() const { return this->width_; }
		[[nodiscard]] uint32_t getHeight() const { return this->height_; }

		[[nodiscard]] rend::Format getFormat() const { return this->format_; }
		[[nodiscard]] uint8_t getBytesPerPixel() const { return rend::getFormatSize(this->format_); }
		[[nodiscard]] uint32_t getRowPitch() const { return this->width_ * this->getBytesPerPixel(); }
		[[nodiscard]] uint32_t getTotalSize() const { return this->getRowPitch() * this->height_; }

		rend::RefCountPtr<ID3D11Texture2D> getTexture() const { return this->texture_; }
	};

	typedef rend::RefCountPtr<rend::Texture2D> Texture2DHandle;
}