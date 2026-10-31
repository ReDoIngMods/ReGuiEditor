#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ReGuiEditor {
	struct Vector3 {
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		constexpr Vector3() = default;
		constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

		constexpr bool operator==(const Vector3& other) const { return this->x == other.x && this->y == other.y && this->z == other.z; }
		constexpr bool operator!=(const Vector3& other) const { return !(*this == other); }

		constexpr Vector3 operator-() const { return { -this->x, -this->y, -this->z }; }

		constexpr Vector3 operator+(const Vector3& other) const { return { this->x + other.x, this->y + other.y, this->z + other.z }; }
		constexpr Vector3 operator-(const Vector3& other) const { return { this->x - other.x, this->y - other.y, this->z - other.z }; }
		constexpr Vector3 operator*(const Vector3& other) const { return { this->x * other.x, this->y * other.y, this->z * other.z }; }
		constexpr Vector3 operator/(const Vector3& other) const { return { this->x / other.x, this->y / other.y, this->z / other.z }; }

		constexpr Vector3 operator*(float scalar) const { return { this->x * scalar, this->y * scalar, this->z * scalar }; }
		constexpr Vector3 operator/(float scalar) const { return { this->x / scalar, this->y / scalar, this->z / scalar }; }

		constexpr float dot(const Vector3& other) const { return this->x * other.x + this->y * other.y + this->z * other.z; }
		constexpr float length2() const { return this->dot(*this); }
		float length() const { return std::sqrt(this->length2()); }

		constexpr Vector3 abs() const { return { std::abs(this->x), std::abs(this->y), std::abs(this->z) }; }
		constexpr Vector3 ceil() const { return { std::ceil(this->x), std::ceil(this->y), std::ceil(this->z) }; }
		constexpr Vector3 floor() const { return { std::floor(this->x), std::floor(this->y), std::floor(this->z) }; }
		constexpr Vector3 min(const Vector3& other) const { return { std::min(this->x, other.x), std::min(this->y, other.y), std::min(this->z, other.z) }; }
		constexpr Vector3 max(const Vector3& other) const { return { std::max(this->x, other.x), std::max(this->y, other.y), std::max(this->z, other.z) }; }

		constexpr Vector3 cross(const Vector3& other) const {
			return {
				this->y * other.z - this->z * other.y,
				this->z * other.x - this->x * other.z,
				this->x * other.y - this->y * other.x
			};
		}

		constexpr Vector3 lerp(const Vector3& other, float t) const { return *this + (other - *this) * t; }

		Vector3 rotate(float angle, const Vector3& axis) const {
			const float c = std::cos(angle);
			const float s = std::sin(angle);

			return *this * c + axis.cross(*this) * s + axis * (axis.dot(*this) * (1.0f - c));
		}

		Vector3 rotateX(float angle) const {
			const float c = std::cos(angle);
			const float s = std::sin(angle);

			return { this->x, this->y * c - this->z * s, this->y * s + this->z * c };
		}

		Vector3 rotateY(float angle) const {
			const float c = std::cos(angle);
			const float s = std::sin(angle);

			return { this->x * c + this->z * s, this->y, -this->x * s + this->z * c };
		}

		Vector3 rotateZ(float angle) const {
			const float c = std::cos(angle);
			const float s = std::sin(angle);

			return { this->x * c - this->y * s, this->x * s + this->y * c, this->z };
		}

		Vector3 normalize() const {
			const float length = this->length();
			if (length < 1e-6f)
				return { 0.0f, 0.0f, 0.0f };

			return *this / length;
		}

		void normalized() {
			const float length = this->length();
			if (length < 1e-6f) {
				this->x = 0.0f;
				this->y = 0.0f;
				this->z = 0.0f;
			} else {
				this->x /= length;
				this->y /= length;
				this->z /= length;
			}
		}

		Vector3 safeNormalize(const Vector3& fallback) const {
			const float length = this->length();
			if (length < 1e-6f)
				return fallback;
		
			return *this / length;
		}

		Vector3 bezier2(const Vector3& control, const Vector3& end, float t) const {
			const float u = 1.0f - t;
			return *this * (u * u) + control * (2.0f * u * t) + end * (t * t);
		}

		Vector3 bezier3(const Vector3& control1, const Vector3& control2, const Vector3& end, float t) const {
			const float u = 1.0f - t;
			return *this * (u * u * u) + control1 * (3.0f * u * u * t) + control2 * (3.0f * u * t * t) + end * (t * t * t);
		}

		static constexpr Vector3 zero() { return { 0.0f, 0.0f, 0.0f }; }
		static constexpr Vector3 one() { return { 1.0f, 1.0f, 1.0f }; }
	
		static constexpr Vector3 forward() { return { 0.0f, -1.0f, 0.0f }; } // Local forward is -Y.
		static constexpr Vector3 up()      { return { 0.0f,  0.0f, 1.0f }; } // Local up      is +Z.
		static constexpr Vector3 right()   { return { 1.0f,  0.0f, 0.0f }; } // Local right   is +X.
	};

	constexpr Vector3 operator*(float scalar, const Vector3& vector) { return vector * scalar; }
}
