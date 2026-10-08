#pragma once
#include "Core/Profiler.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ProfilerModel {

enum class LaneFilter { All, CPU, GPU };

constexpr uint64_t kKeyFrame = 1;
constexpr uint64_t kKeyWork = 2;
constexpr uint64_t kKeyWait = 3;
constexpr uint64_t kKeyUnaccounted = 4;
constexpr float kBudgetUs60Fps = 1000000.0f / 60.0f;

struct Row {
    uint64_t key = 0;
    uint64_t parentKey = 0;
    std::string name;
    int depth = 0;
    bool hasChildren = false;
    bool expanded = false;
    bool synthetic = false;
    bool isWait = false;
    Profiler::Lane lane = Profiler::Lane::CPU;
    int64_t totalNs = 0;
    int64_t selfNs = 0;
    uint32_t calls = 0;
    double percentOfWork = -1.0;
    float avgUs = 0.0f;
    float maxUs = 0.0f;
};

struct BuildOptions {
    LaneFilter laneFilter = LaneFilter::All;
    const std::unordered_set<uint64_t>* expanded = nullptr;
    size_t historyWindow = Profiler::kHistoryFrames;
};

void BuildRows(const Profiler::Frame& frame, const BuildOptions& options, std::vector<Row>& out);

void ExpandHottestPath(const Profiler::Frame& frame, std::unordered_set<uint64_t>& expanded);

std::vector<uint64_t> SelectLineKeys(const std::vector<Row>& rows, size_t topN,
                                     const std::unordered_set<uint64_t>& pinned,
                                     const std::unordered_set<uint64_t>& hidden);

void FillSeries(uint64_t key, size_t window, std::vector<float>& outUs);

float NiceMaxUs(float valueUs);

class ColorAllocator {
public:
    explicit ColorAllocator(size_t paletteSize) : paletteSize(paletteSize ? paletteSize : 1) {}
    size_t Get(uint64_t key);
    void Reset();

private:
    size_t paletteSize;
    size_t next = 0;
    std::unordered_map<uint64_t, size_t> assigned;
};

}
