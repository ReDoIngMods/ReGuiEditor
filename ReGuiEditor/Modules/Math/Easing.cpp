#include "Easing.hpp"

#include "Constants.hpp"

#include <array>
#include <cmath>
#include <cstring>

using namespace ReGuiEditor;

template<int Power> 
inline static double easeInPow(double t) { return std::pow(t, Power); }

template<int Power>
inline static double easeOutPow(double t) { return 1.0 - std::pow(1.0 - t, Power); }

template<int Power>
inline static double easeInOutPow(double t) {
	return t < 0.5 ? std::pow(2.0, Power - 1) * std::pow(t, Power) : 1.0 - std::pow(-2.0 * t + 2.0, Power) / 2.0;
}

constexpr double BACK_C1 = 1.70158;
constexpr double BACK_C2 = BACK_C1 * 1.525;
constexpr double BACK_C3 = BACK_C1 + 1.0;

constexpr double ELASTIC_C4 = (2.0 * PI_DOUBLE) / 3.0;
constexpr double ELASTIC_C5 = (2.0 * PI_DOUBLE) / 4.5;

inline static double calculateBounceOut(double t) {
	constexpr double n1 = 7.5625, d1 = 2.75;
	if (t < 1.0 / d1)
		return n1 * t * t;

	if (t < 2.0 / d1) {
		const double u = t - 1.5 / d1;
		return n1 * u * u + 0.75;
	}

	if (t < 2.5 / d1) {
		const double u = t - 2.25 / d1;
		return n1 * u * u + 0.9375;
	}

	const double u = t - 2.625 / d1;
	return n1 * u * u + 0.984375;
}

struct EasingStruct {
	const char* name;
	double (*function)(double);
};

constexpr auto EASING_FUNCTIONS = std::to_array<EasingStruct>({
	{ "linear", [](double t) -> double { return t; } },
	
	{ "easeInSine", [](double t) -> double { return 1.0 - std::cos(t * PI_2_DOUBLE); } },
	{ "easeOutSine", [](double t) -> double { return std::sin(t * PI_2_DOUBLE); } },
	{ "easeInOutSine", [](double t) -> double { return -(std::cos(PI_DOUBLE * t) - 1.0) / 2.0; } },

	{ "easeInQuad", easeInPow<2> },
	{ "easeOutQuad", easeOutPow<2> },
	{ "easeInOutQuad", easeInOutPow<2> },

	{ "easeInCubic", easeInPow<3> },
	{ "easeOutCubic", easeOutPow<3> },
	{ "easeInOutCubic", easeInOutPow<3> },

	{ "easeInQuart", easeInPow<4> },
	{ "easeOutQuart", easeOutPow<4> },
	{ "easeInOutQuart", easeInOutPow<4> },

	{ "easeInQuint", easeInPow<5> },
	{ "easeOutQuint", easeOutPow<5> },
	{ "easeInOutQuint", easeInOutPow<5> },

	{ "easeInExpo", [](double t) { return t == 0.0 ? 0.0 : std::pow(2.0, 10.0 * t - 10.0); } },
	{ "easeOutExpo", [](double t) { return t == 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * t); } },
	{ "easeInOutExpo", [](double t) {
		if (t == 0.0 || t == 1.0) return t;
		if (t  < 0.5)             return std::pow(2.0, 20.0 * t - 10.0) / 2.0;
		else                      return (2.0 - std::pow(2.0, -20.0 * t + 10.0)) / 2.0;
	} },

	{ "easeInCirc", [](double t) { return 1.0 - std::sqrt(1.0 - t * t); } },
	{ "easeOutCirc", [](double t) { return std::sqrt(1.0 - (t - 1.0) * (t - 1.0)); } },
	{ "easeInOutCirc", [](double t) {
		if (t < 0.5) return (1.0 - std::sqrt(1.0 - 4.0 * t * t)) / 2.0;
		else         return (std::sqrt(1.0 - (-2.0 * t + 2.0) * (-2.0 * t + 2.0)) + 1.0) / 2.0;
	} },

	{ "easeInBack", [](double t) { return BACK_C3 * t * t * t - BACK_C1 * t * t; } },
	{ "easeOutBack", [](double t) { return 1.0 + BACK_C3 * std::pow(t - 1.0, 3.0) + BACK_C1 * std::pow(t - 1.0, 2.0); } },
	{ "easeInOutBack", [](double t) {
		if (t < 0.5) return (std::pow(2.0 * t, 2.0) * ((BACK_C2 + 1.0) * 2.0 * t - BACK_C2)) / 2.0;
		else         return (std::pow(2.0 * t - 2.0, 2.0) * ((BACK_C2 + 1.0) * (t * 2.0 - 2.0) + BACK_C2) + 2.0) / 2.0;
	} },

	{ "easeInElastic", [](double t) {
		if (t == 0.0 || t == 1.0) return t;
		else                      return -std::pow(2.0, 10.0 * t - 10.0) * std::sin((t * 10.0 - 10.75) * ELASTIC_C4);
	} },

	{ "easeOutElastic", [](double t) {
		if (t == 0.0 || t == 1.0) return t;
		else                      return std::pow(2.0, -10.0 * t) * std::sin((t * 10.0 - 0.75) * ELASTIC_C4) + 1.0;
	} },

	{ "easeInOutElastic", [](double t) {
		if (t == 0.0 || t == 1.0) return t;
		if (t <  0.5)             return -(std::pow(2.0, 20.0 * t - 10.0) * std::sin((20.0 * t - 11.125) * ELASTIC_C5)) / 2.0;
		else                      return (std::pow(2.0, -20.0 * t + 10.0) * std::sin((20.0 * t - 11.125) * ELASTIC_C5)) / 2.0 + 1.0;
	} },

	{ "easeInBounce", [](double t) { return 1.0 - calculateBounceOut(1.0 - t); } },
	{ "easeOutBounce", [](double t) { return calculateBounceOut(t); } },
	{ "easeInOutBounce", [](double t) {
		if (t < 0.5) return (1.0 - calculateBounceOut(1.0 - 2.0 * t)) / 2.0;
		else         return (1.0 + calculateBounceOut(2.0 * t - 1.0)) / 2.0;
	} },
});

double ReGuiEditor::easingCalculate(double t, EasingType easingType) {
	size_t index = static_cast<size_t>(easingType);
	if (index >= EASING_FUNCTIONS.size())
		return t;

	return EASING_FUNCTIONS[index].function(t);
}

double ReGuiEditor::easingCalculate(double t, const char* easingTypeName) {
	for (const auto& easing : EASING_FUNCTIONS) {
		if (strcmp(easing.name, easingTypeName) == 0)
			return easing.function(t);
	}

	return t;
}

bool ReGuiEditor::easingTypenameValid(const char* easingTypeName) {
	for (const auto& easing : EASING_FUNCTIONS) {
		if (strcmp(easing.name, easingTypeName) == 0)
			return true;
	}

	return false;
}

bool ReGuiEditor::easingTypeidValid(int easingTypeId) {
	return easingTypeId >= 0 && easingTypeId < static_cast<int>(EASING_FUNCTIONS.size());
}