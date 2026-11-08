#include "DebugTextHandler.hpp"

#include <rend/Common/RefCounter.hpp>
#include <rend/D3D11Device.hpp>

#include <ImGui/imgui.h>

#include <UTILS/Profiler.hpp>
#include <UTILS/ValueAverager.hpp>

#include <Windows.h>
#include <dxgi1_4.h>

#include <Psapi.h>
#pragma comment(lib, "psapi.lib")

#include <cfloat>

struct FrameStats {
	UTILS::ValueAverager<float> fpsAverager;
	UTILS::ValueAverager<float> frameTimeAverager;

	float minFrameTime = FLT_MAX;
	float maxFrameTime = -FLT_MAX;

	float cpuMemoryMb = 0.0f;
	float gpuMemoryMb = 0.0f;
	float gpuBudgetMb = 0.0f;
private:
	static constexpr float BYTES_TO_MB = 1.0f / (1024.0f * 1024.0f);
	static constexpr float MEMORY_QUERY_INTERVAL = 0.5f;
	float memoryQueryTimer = MEMORY_QUERY_INTERVAL;

	void updateMemoryUsage() {
		PROCESS_MEMORY_COUNTERS_EX2 counters;
		ZeroMemory(&counters, sizeof(counters));
		if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
			this->cpuMemoryMb = counters.PrivateUsage * BYTES_TO_MB;

		rend::D3D11Device& device = rend::D3D11Device::getInstance();
		rend::RefCountPtr<IDXGIAdapter3> adapter3 = device.getAdapter3();

		DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
		ZeroMemory(&videoMemoryInfo, sizeof(videoMemoryInfo));

		HRESULT result = adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo);
		if (SUCCEEDED(result)) {
			this->gpuMemoryMb = videoMemoryInfo.CurrentUsage * BYTES_TO_MB;
			this->gpuBudgetMb = videoMemoryInfo.Budget       * BYTES_TO_MB;
		}
	}
public:
	void update(float deltaTime) {
		this->fpsAverager.update(1.0f / deltaTime);
		this->frameTimeAverager.update(deltaTime * 1000.0f);

		if (deltaTime < this->minFrameTime) this->minFrameTime = deltaTime;
		if (deltaTime > this->maxFrameTime) this->maxFrameTime = deltaTime;

		this->memoryQueryTimer += deltaTime;
		if (this->memoryQueryTimer >= MEMORY_QUERY_INTERVAL) {
			this->updateMemoryUsage();
			this->memoryQueryTimer = 0.0f;
		}
	}
};

void DebugTextHandler::initialize() {
	static FrameStats stats;
	this->addEntry("General Info", [](DebugTextStream& stream) mutable {
		const ImGuiIO& io = ImGui::GetIO();
		stats.update(io.DeltaTime);

		stream.print("FPS        = %.1f", stats.fpsAverager.getAverage());
		stream.print("Frame Time = %.2f ms (min %.2f | max %.2f)", stats.frameTimeAverager.getAverage(), stats.minFrameTime, stats.maxFrameTime);

		const int uptime = static_cast<int>(ImGui::GetTime());
		stream.print("Uptime     = %02d:%02d:%02d (frame %d)", uptime / 3600, uptime / 60 % 60, uptime % 60, ImGui::GetFrameCount());

		stream.print("CPU Memory = %.2f MB", stats.cpuMemoryMb);
		stream.print("GPU Memory = %.2f MB / %.2f MB", stats.gpuMemoryMb, stats.gpuBudgetMb);
		stream.print("Display    = %.0fx%.0f @ %.2fx", io.DisplaySize.x, io.DisplaySize.y, io.DisplayFramebufferScale.x);

		if (ImGui::IsMousePosValid())
			stream.print("Mouse      = (%.0f, %.0f)", io.MousePos.x, io.MousePos.y);
		else
			stream.print("Mouse      = N/A");

		stream.print("ImGui      = %d windows, %d vtx, %d idx", io.MetricsRenderWindows, io.MetricsRenderVertices, io.MetricsRenderIndices);
	});

	this->addEntry("Profiler info", [](DebugTextStream& stream) {
		if (UTILS::gProfilerResults.empty()) {
			stream.write("No profiler results available.");
			return;
		}

		size_t nameColumnWidth = 0;
		for (const auto& [name, _] : UTILS::gProfilerResults) {
			if (name.size() > nameColumnWidth)
				nameColumnWidth = name.size();
		}

		for (const auto& [name, time] : UTILS::gProfilerResults) {
			const double averageTime = time.getAverage();
			const size_t sampleCount = time.getSampleCount();
			
			const char* timeUnit     = (averageTime > 1000.0) ? "s"                  : (averageTime > 1.0) ? "ms"        : "us";
			const double displayTime = (averageTime > 1000.0) ? averageTime / 1000.0 : (averageTime > 1.0) ? averageTime : averageTime * 1000.0;
			
			if (sampleCount == UTILS::VALUE_AVERAGER_SAMPLE_COUNT)
				stream.print("%-*s = [MAX S] %10.3f %s", static_cast<int>(nameColumnWidth), name.c_str(), displayTime, timeUnit);
			else
				stream.print("%-*s = [%3d S] %10.3f %s", static_cast<int>(nameColumnWidth), name.c_str(), static_cast<int>(sampleCount), displayTime, timeUnit);
		}
	});
}
