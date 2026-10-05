#pragma once
#include <type_traits>

namespace rend {
	template<typename T>
	T alignTo(T value, T alignment) {
		static_assert(std::is_integral_v<T>, "alignTo requires an integral type");
		return (value + alignment - 1) & ~(alignment - 1);
	}

	template<typename T>
	T alignPower2(T value) {
		static_assert(std::is_integral_v<T>, "alignPower2 requires an integral type");
		
		if (value == 0)
			return 1;

		--value;

		for (size_t index = 1; index < sizeof(T) * 8; index <<= 1)
			value |= value >> index;

		return value + 1;
	}
}