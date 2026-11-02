#pragma once
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace ReGuiEditor {
	template<typename Function>
	void parallelFor(size_t count, Function&& fn) {
		if (count == 0)
			return;

		std::atomic<size_t> nextIndex = 0;
		const auto worker = [&fn, &nextIndex, count]() {
			for (size_t index = nextIndex.fetch_add(1); index < count; index = nextIndex.fetch_add(1))
				fn(index);
		};

		const size_t hardwareThreads = std::max<size_t>(1, std::thread::hardware_concurrency());
		const size_t workerCount = std::min(count, hardwareThreads);

		std::vector<std::jthread> threads;
		threads.reserve(workerCount - 1);

		for (size_t index = 1; index < workerCount; ++index)
			threads.emplace_back(worker);

		worker();
	}
}
