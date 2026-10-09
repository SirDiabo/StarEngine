#pragma once
#include "Profiler.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ProfilerRuntime {

struct Options {
    bool enabled = false;
    bool capture = true;
    bool overlay = false;
    bool hotkeys = true;
    int detail = Profiler::DetailStandard;
    std::string outputPath;
    std::vector<std::string> warnings;
};

struct Hotspot {
    std::string name;
    int64_t selfNs = 0;
    int64_t totalNs = 0;
    Profiler::Lane lane = Profiler::Lane::CPU;
};

Options ParseArguments(int argc, const char* const* argv, const char* environmentValue);
std::string DefaultCapturePath(const std::string& appName);
std::vector<Hotspot> ComputeHotspots(const Profiler::Frame& frame, size_t limit);

bool Initialize(const Options& options, const std::string& appName);
void BeginFrame();
void PreEndDrawing();
void DrawOverlay();
void Shutdown();
bool IsEnabled();
bool IsOverlayVisible();

}
