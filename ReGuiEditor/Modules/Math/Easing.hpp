#pragma once
#include <cstdint>

namespace ReGuiEditor {
	enum class EasingType : uint8_t {
		Linear,

		EaseInSine,
		EaseOutSine,
		EaseInOutSine,

		EaseInQuad,
		EaseOutQuad,
		EaseInOutQuad,

		EaseInCubic,
		EaseOutCubic,
		EaseInOutCubic,

		EaseInQuart,
		EaseOutQuart,
		EaseInOutQuart,

		EaseInQuint,
		EaseOutQuint,
		EaseInOutQuint,

		EaseInExpo,
		EaseOutExpo,
		EaseInOutExpo,

		EaseInCirc,
		EaseOutCirc,
		EaseInOutCirc,

		EaseInBack,
		EaseOutBack,
		EaseInOutBack,

		EaseInElastic,
		EaseOutElastic,
		EaseInOutElastic,

		EaseInBounce,
		EaseOutBounce,
		EaseInOutBounce,

		COUNT
	};

	double easingCalculate(double t, EasingType easingType);
	double easingCalculate(double t, const char* easingName);

	bool easingTypenameValid(const char* easingName);
	bool easingTypeidValid(int easingTypeId);

	template<typename T>
	inline T lerp(const T& a, const T& b, T t) {
		return a + (b - a) * t;
	}

	template<typename T>
	inline T fade(const T& t) {
		return t * t * t * (t * (t * 6 - 15) + 10);
	}
}