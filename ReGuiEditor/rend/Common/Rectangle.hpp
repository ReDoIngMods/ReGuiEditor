#pragma once

namespace rend {
	struct Rectangle {
		float left;
		float top;
		float right;
		float bottom;

		constexpr Rectangle() : left(0), top(0), right(0), bottom(0) {}
		constexpr Rectangle(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}

		constexpr bool operator==(const rend::Rectangle& other) const {
			return this->left == other.left && this->top == other.top && this->right == other.right && this->bottom == other.bottom;
		}

		constexpr bool operator!=(const rend::Rectangle& other) const {
			return !(*this == other);
		}

		constexpr bool contains(float x, float y) const {
			return x >= this->left && x <= this->right && y >= this->top && y <= this->bottom;
		}

		constexpr bool intersects(const rend::Rectangle& other) const {
			return !(other.left > this->right || other.right < this->left || other.top > this->bottom || other.bottom < this->top);
		}

		constexpr float width() const { return this->right - this->left; }
		constexpr float height() const { return this->bottom - this->top; }

		constexpr float area() const { return this->width() * this->height(); }
	};
}