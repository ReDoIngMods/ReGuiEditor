#pragma once
#include <stack>
#include <type_traits>

namespace UTILS {
	static constexpr size_t VALUE_AVERAGER_SAMPLE_COUNT = 256;

	template<typename T>
	class ValueAverager {
		static_assert(std::is_arithmetic_v<T>, "ValueAverager can only be used with arithmetic types.");

		std::stack<T> values_;
		T sumValue_ = T();
		T lastValue_ = T();
	public:
		void update(const T& newValue) {
			if (this->values_.size() >= VALUE_AVERAGER_SAMPLE_COUNT) {
				this->sumValue_ -= this->values_.top();
				this->values_.pop();
			}
		
			this->values_.push(newValue);
			this->sumValue_ += newValue;
			this->lastValue_ = newValue;
		}

		T getAverage() const {
			if (this->values_.empty())
				return T();
		
			return this->sumValue_ / static_cast<T>(this->values_.size());
		}

		T getLastValue() const {
			return this->lastValue_;
		}

		size_t getSampleCount() const {
			return this->values_.size();
		}
	};
}