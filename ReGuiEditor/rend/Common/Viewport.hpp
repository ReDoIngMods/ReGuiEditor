#pragma once

namespace rend {
	struct Viewport {
		float x;
		float y;
		float width;
		float height;
		float minDepth;
		float maxDepth;

		constexpr Viewport() : x(0), y(0), width(0), height(0), minDepth(0), maxDepth(1) {}
		constexpr Viewport(float x, float y, float width, float height, float minDepth, float maxDepth) : x(x), y(y), width(width), height(height), minDepth(minDepth), maxDepth(maxDepth) {}

		constexpr bool operator==(const rend::Viewport& other) const {
			return this->x == other.x && this->y == other.y && this->width == other.width && this->height == other.height && this->minDepth == other.minDepth && this->maxDepth == other.maxDepth;
		}

		constexpr bool operator!=(const rend::Viewport& other) const {
			return !(*this == other);
		}
	};
}