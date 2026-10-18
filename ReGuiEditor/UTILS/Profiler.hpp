#pragma once
#include "ValueAverager.hpp"

#include <Windows.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace UTILS {
	extern std::vector<std::pair<std::string, UTILS::ValueAverager<double>>> gProfilerResults;

	class Profiler {
		LARGE_INTEGER frequency_;
		LARGE_INTEGER startTime_;
		LARGE_INTEGER endTime_;
	public:
		Profiler() {
			QueryPerformanceFrequency(&this->frequency_);
			QueryPerformanceCounter(&this->startTime_);
			this->endTime_ = this->startTime_;
		}

		void start() {
			this->endTime_ = this->startTime_;
			QueryPerformanceCounter(&this->startTime_);
		}

		void stop() {
			QueryPerformanceCounter(&this->endTime_);
		}

		double getElapsedSeconds() const {
			return static_cast<double>(this->endTime_.QuadPart - this->startTime_.QuadPart) / static_cast<double>(this->frequency_.QuadPart);
		}

		double getElapsedMilliseconds() const {
			return this->getElapsedSeconds() * 1000.0;
		}

		void saveResult(const std::string& name) const {
			for (auto& [existingName, averager] : UTILS::gProfilerResults) {
				if (existingName == name) {
					averager.update(this->getElapsedMilliseconds());
					return;
				}
			}

			UTILS::gProfilerResults.emplace_back(name, UTILS::ValueAverager<double>());
			UTILS::gProfilerResults.back().second.update(this->getElapsedMilliseconds());

			std::sort(UTILS::gProfilerResults.begin(), UTILS::gProfilerResults.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
		}
	};

	class ScopedProfiler {
		Profiler profiler_;
		std::string name_;
	public:
		ScopedProfiler(const std::string& name) : name_(name) {
			this->profiler_.start();
		}

		~ScopedProfiler() {
			this->profiler_.stop();
			this->profiler_.saveResult(this->name_);
		}
	};
}

#define PROFILE_CONCAT_IMPL(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_IMPL(a, b)

#define PROFILE_SCOPE(name) UTILS::ScopedProfiler PROFILE_CONCAT(profiler, __LINE__)(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)