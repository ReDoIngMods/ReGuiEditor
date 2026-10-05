#pragma once
#include <cstdint>

namespace rend {
	union Color {
		struct {
			float r;
			float g;
			float b;
			float a;
		};
		
		float components[4];

		constexpr Color() : r(0), g(0), b(0), a(1) {}
		constexpr Color(float red, float green, float blue, float alpha = 1.0f) : r(red), g(green), b(blue), a(alpha) {}

		constexpr bool operator==(const Color& other) const { return this->r == other.r && this->g == other.g && this->b == other.b && this->a == other.a; }
		constexpr bool operator!=(const Color& other) const { return !(*this == other); }

		constexpr auto operator<=>(const Color& other) const {
			if (this->r != other.r) return this->r <=> other.r;
			if (this->g != other.g) return this->g <=> other.g;
			if (this->b != other.b) return this->b <=> other.b;
			return this->a <=> other.a;
		}

		constexpr rend::Color operator+(const rend::Color& other) const { return { this->r + other.r, this->g + other.g, this->b + other.b, this->a + other.a }; }
		constexpr rend::Color operator-(const rend::Color& other) const { return { this->r - other.r, this->g - other.g, this->b - other.b, this->a - other.a }; }
		constexpr rend::Color operator*(const rend::Color& other) const { return { this->r * other.r, this->g * other.g, this->b * other.b, this->a * other.a }; }
		constexpr rend::Color operator/(const rend::Color& other) const { return { this->r / other.r, this->g / other.g, this->b / other.b, this->a / other.a }; }

		constexpr rend::Color operator*(float scalar) const { return { this->r * scalar, this->g * scalar, this->b * scalar, this->a * scalar }; }
		constexpr rend::Color operator/(float scalar) const { return { this->r / scalar, this->g / scalar, this->b / scalar, this->a / scalar }; }

		constexpr uint32_t toRGBA() const {
			const uint32_t rInt = static_cast<uint32_t>(this->r * 255.0f);
			const uint32_t gInt = static_cast<uint32_t>(this->g * 255.0f);
			const uint32_t bInt = static_cast<uint32_t>(this->b * 255.0f);
			const uint32_t aInt = static_cast<uint32_t>(this->a * 255.0f);
		
			return (aInt << 24) | (bInt << 16) | (gInt << 8) | rInt;
		}

		constexpr static rend::Color fromRGBA(uint32_t rgba) {
			return {
				((rgba >> 0) & 0xFF) / 255.0f,
				((rgba >> 8) & 0xFF) / 255.0f,
				((rgba >> 16) & 0xFF) / 255.0f,
				((rgba >> 24) & 0xFF) / 255.0f
			};
		}

		constexpr static rend::Color lerp(const rend::Color& color1, const rend::Color& color2, float t) {
			return color1 + (color2 - color1) * t;
		}

		constexpr static rend::Color white()       { return { 1.00f, 1.00f, 1.00f, 1.00f }; }
		constexpr static rend::Color black()       { return { 0.00f, 0.00f, 0.00f, 1.00f }; }
		constexpr static rend::Color red()         { return { 1.00f, 0.00f, 0.00f, 1.00f }; }
		constexpr static rend::Color green()       { return { 0.00f, 1.00f, 0.00f, 1.00f }; }
		constexpr static rend::Color blue()        { return { 0.00f, 0.00f, 1.00f, 1.00f }; }
		constexpr static rend::Color yellow()      { return { 1.00f, 1.00f, 0.00f, 1.00f }; }
		constexpr static rend::Color cyan()        { return { 0.00f, 1.00f, 1.00f, 1.00f }; }
		constexpr static rend::Color magenta()     { return { 1.00f, 0.00f, 1.00f, 1.00f }; }
		constexpr static rend::Color gray()        { return { 0.50f, 0.50f, 0.50f, 1.00f }; }
		constexpr static rend::Color darkGray()    { return { 0.25f, 0.25f, 0.25f, 1.00f }; }
		constexpr static rend::Color lightGray()   { return { 0.75f, 0.75f, 0.75f, 1.00f }; }
		constexpr static rend::Color transparent() { return { 0.00f, 0.00f, 0.00f, 0.00f }; }
		constexpr static rend::Color orange()      { return { 1.00f, 0.50f, 0.00f, 1.00f }; }
		constexpr static rend::Color purple()      { return { 0.50f, 0.00f, 1.00f, 1.00f }; }
		constexpr static rend::Color pink()        { return { 1.00f, 0.40f, 0.70f, 1.00f }; }
		constexpr static rend::Color brown()       { return { 0.60f, 0.30f, 0.10f, 1.00f }; }
		constexpr static rend::Color lime()        { return { 0.75f, 1.00f, 0.00f, 1.00f }; }
	};
}