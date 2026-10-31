#include "DebugTextHandler.hpp"

#include <Modules/Math/ValueAverager.hpp>
#include <Modules/Misc/Profiler.hpp>

#include <ImGui/imgui.h>

#include <Windows.h>

#include <Psapi.h>
#pragma comment(lib, "psapi.lib")

#include <cfloat>

struct FrameStats {
	ReGuiEditor::ValueAverager<float> fpsAverager;
	ReGuiEditor::ValueAverager<float> frameTimeAverager;

	float minFrameTime = FLT_MAX;
	float maxFrameTime = -FLT_MAX;
	float workingSetMegabytes = 0.0f;
private:
	[[nodiscard]] static float queryWorkingSetMb() {
		PROCESS_MEMORY_COUNTERS counters;
		ZeroMemory(&counters, sizeof(counters));
		if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
			return 0.0f;

		return static_cast<float>(counters.WorkingSetSize) / (1024.0f * 1024.0f);
	}
public:
	void update(float deltaTime) {
		this->fpsAverager.update(1.0f / deltaTime);
		this->frameTimeAverager.update(deltaTime * 1000.0f);

		if (deltaTime < this->minFrameTime) this->minFrameTime = deltaTime;
		if (deltaTime > this->maxFrameTime) this->maxFrameTime = deltaTime;

		this->workingSetMegabytes = queryWorkingSetMb();
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

		stream.print("Memory     = %.1f MB", stats.workingSetMegabytes);
		stream.print("Display    = %.0fx%.0f @ %.2fx", io.DisplaySize.x, io.DisplaySize.y, io.DisplayFramebufferScale.x);

		if (ImGui::IsMousePosValid())
			stream.print("Mouse      = (%.0f, %.0f)", io.MousePos.x, io.MousePos.y);
		else
			stream.print("Mouse      = N/A");

		stream.print("ImGui      = %d windows, %d vtx, %d idx", io.MetricsRenderWindows, io.MetricsRenderVertices, io.MetricsRenderIndices);
	});

	this->addEntry("Profiler info", [](DebugTextStream& stream) {
		if (ReGuiEditor::gProfilerResults.empty()) {
			stream.write("No profiler results available.");
			return;
		}

		size_t nameColumnWidth = 0;
		for (const auto& [name, _] : ReGuiEditor::gProfilerResults) {
			if (name.size() > nameColumnWidth)
				nameColumnWidth = name.size();
		}

		for (const auto& [name, time] : ReGuiEditor::gProfilerResults) {
			const double averageTime = time.getAverage();
			const size_t sampleCount = time.getSampleCount();
			
			const char* timeUnit     = (averageTime > 1000.0) ? "s"                  : (averageTime > 1.0) ? "ms"        : "us";
			const double displayTime = (averageTime > 1000.0) ? averageTime / 1000.0 : (averageTime > 1.0) ? averageTime : averageTime * 1000.0;
			
			if (sampleCount == ReGuiEditor::VALUE_AVERAGER_SAMPLE_COUNT)
				stream.print("%-*s = [MAX S] %10.3f %s", static_cast<int>(nameColumnWidth), name.c_str(), displayTime, timeUnit);
			else
				stream.print("%-*s = [%3d S] %10.3f %s", static_cast<int>(nameColumnWidth), name.c_str(), static_cast<int>(sampleCount), displayTime, timeUnit);
		}
	});
}
