#pragma once

namespace ReGuiEditor {
	constexpr double PI_DOUBLE = 3.14159265358979323846;
	constexpr double PI_2_DOUBLE = PI_DOUBLE / 2.0;
	constexpr double DEGREES_TO_RADIANS_DOUBLE = PI_DOUBLE / 180.0;

	constexpr float PI_FLOAT = static_cast<float>(PI_DOUBLE);
	constexpr float PI_2_FLOAT = static_cast<float>(PI_2_DOUBLE);
	constexpr float DEGREES_TO_RADIANS_FLOAT = static_cast<float>(DEGREES_TO_RADIANS_DOUBLE);
}