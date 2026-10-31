#pragma once
#include "Vector3.hpp"

#include <cmath>
#include <cstdlib>

namespace ReGuiEditor {
	struct Quaternion {
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 1.0f;

		constexpr Quaternion() = default;
		constexpr Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

		constexpr bool operator==(const Quaternion& other) const { return this->x == other.x && this->y == other.y && this->z == other.z && this->w == other.w; }
		constexpr bool operator!=(const Quaternion& other) const { return !(*this == other); }

		constexpr Quaternion operator*(const Quaternion& other) const {
			return {
				this->w * other.x + this->x * other.w + this->y * other.z - this->z * other.y,
				this->w * other.y - this->x * other.z + this->y * other.w + this->z * other.x,
				this->w * other.z + this->x * other.y - this->y * other.x + this->z * other.w,
				this->w * other.w - this->x * other.x - this->y * other.y - this->z * other.z
			};
		}

		constexpr Vector3 operator*(const Vector3& vector) const {
			const Vector3 axis(this->x, this->y, this->z);
			const Vector3 t = axis.cross(vector) * 2.0f;

			return vector + t * this->w + axis.cross(t);
		}

		constexpr Quaternion operator/(float scalar) const { return { this->x / scalar, this->y / scalar, this->z / scalar, this->w / scalar }; }
		constexpr Quaternion operator*(float scalar) const { return { this->x * scalar, this->y * scalar, this->z * scalar, this->w * scalar }; }

		constexpr float dot(const Quaternion& other) const { return this->x * other.x + this->y * other.y + this->z * other.z + this->w * other.w; }
		constexpr float length2() const { return this->dot(*this); }
		float length() const { return std::sqrt(this->length2()); }

		constexpr Quaternion conjugate() const { return { -this->x, -this->y, -this->z, this->w }; }

		Quaternion inverse() const {
			const float length2 = this->length2();
			if (length2 <= 0.0f)
				return identity();

			return this->conjugate() / length2;
		}

		Quaternion normalize() const {
			const float length = this->length();
			if (length <= 0.0f)
				return identity();

			return { this->x / length, this->y / length, this->z / length, this->w / length };
		}

		constexpr Vector3 getForward() const { return *this * Vector3::forward(); }
		constexpr Vector3 getUp() const { return *this * Vector3::up(); }
		constexpr Vector3 getRight() const { return *this * Vector3::right(); }

		constexpr static Quaternion identity() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }

		static Quaternion angleAxis(float angle, const Vector3& axis) {
			const float length = axis.length();
			if (length <= 0.0f)
				return Quaternion::identity();

			const float half = angle * 0.5f;
			const float s = std::sin(half) / length;

			return { axis.x * s, axis.y * s, axis.z * s, std::cos(half) };
		}

		static Quaternion fromEuler(const Vector3& angles) {
			return Quaternion::angleAxis(angles.z, { 0.0f, 0.0f, 1.0f })
				 * Quaternion::angleAxis(angles.y, { 0.0f, 1.0f, 0.0f })
				 * Quaternion::angleAxis(angles.x, { 1.0f, 0.0f, 0.0f });
		}

		static Quaternion fromTo(const Vector3& from, const Vector3& to) {
			const float lengths = from.length() * to.length();
			if (lengths <= 0.0f)
				return Quaternion::identity();

			const float cosine = from.dot(to) / lengths;
			if (cosine >= 1.0f - 1e-6f)
				return Quaternion::identity();

			if (cosine <= -1.0f + 1e-6f) {
				const Vector3 helper = std::abs(from.x) < std::abs(from.y) ? Vector3(1.0f, 0.0f, 0.0f) : Vector3(0.0f, 1.0f, 0.0f);
				return Quaternion::angleAxis(3.14159265358979f, from.cross(helper));
			}

			const Vector3 axis = from.cross(to);
			return Quaternion(axis.x, axis.y, axis.z, lengths + from.dot(to)).normalize();
		}

		static Quaternion fromAxes(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis) {
			const float trace = xAxis.x + yAxis.y + zAxis.z;
			if (trace > 0.0f) {
				const float s = std::sqrt(trace + 1.0f) * 2.0f;
				return { (yAxis.z - zAxis.y) / s, (zAxis.x - xAxis.z) / s, (xAxis.y - yAxis.x) / s, 0.25f * s };
			}

			if (xAxis.x > yAxis.y && xAxis.x > zAxis.z) {
				const float s = std::sqrt(1.0f + xAxis.x - yAxis.y - zAxis.z) * 2.0f;
				return { 0.25f * s, (yAxis.x + xAxis.y) / s, (zAxis.x + xAxis.z) / s, (yAxis.z - zAxis.y) / s };
			}

			if (yAxis.y > zAxis.z) {
				const float s = std::sqrt(1.0f + yAxis.y - xAxis.x - zAxis.z) * 2.0f;
				return { (yAxis.x + xAxis.y) / s, 0.25f * s, (zAxis.y + yAxis.z) / s, (zAxis.x - xAxis.z) / s };
			}

			const float s = std::sqrt(1.0f + zAxis.z - xAxis.x - yAxis.y) * 2.0f;
			return { (zAxis.x + xAxis.z) / s, (zAxis.y + yAxis.z) / s, 0.25f * s, (xAxis.y - yAxis.x) / s };
		}

		static Quaternion lookRotation(const Vector3& forward, const Vector3& up) {
			if (forward.length2() <= 0.0f)
				return identity();

			const Vector3 f = forward / forward.length();

			Vector3 right = up.cross(f);
			if (right.length2() <= 1e-12f)
				right = std::abs(f.x) < 0.9f ? Vector3(1.0f, 0.0f, 0.0f).cross(f) : Vector3(0.0f, 1.0f, 0.0f).cross(f);

			right = right / right.length();

			return fromAxes(right, -f, f.cross(right));
		}

		static Quaternion slerp(const Quaternion& from, Quaternion to, float t) {
			float cosine = from.dot(to);
			if (cosine < 0.0f) {
				to = { -to.x, -to.y, -to.z, -to.w };
				cosine = -cosine;
			}

			float a, b;
			if (cosine > 0.9995f) {
				a = 1.0f - t;
				b = t;
			} else {
				const float theta = std::acos(cosine);
				const float sine = std::sin(theta);

				a = std::sin((1.0f - t) * theta) / sine;
				b = std::sin(t * theta) / sine;
			}

			return Quaternion(from.x * a + to.x * b, from.y * a + to.y * b, from.z * a + to.z * b, from.w * a + to.w * b).normalize();
		}
	};
}

inline static ReGuiEditor::Quaternion operator*(float scalar, const ReGuiEditor::Quaternion& quaternion) { return quaternion * scalar; }
inline static ReGuiEditor::Vector3 operator*(const ReGuiEditor::Vector3& vector, const ReGuiEditor::Quaternion& quaternion) { return quaternion * vector; }