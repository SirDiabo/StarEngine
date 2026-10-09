#include "ProfilerModel.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ProfilerModel {

namespace {

struct Collector {
    const Profiler::Frame& frame;
    const BuildOptions& options;
    std::vector<std::vector<int>> children;
    int64_t workNs;

    Collector(const Profiler::Frame& f, const BuildOptions& o)
        : frame(f), options(o), children(f.nodes.size()), workNs(f.workNs) {}

    bool LaneAllowed(const Profiler::Node& node) const {
        if (options.laneFilter == LaneFilter::All) return true;
        if (options.laneFilter == LaneFilter::CPU) return node.lane == Profiler::Lane::CPU;
        return node.lane == Profiler::Lane::GPU;
    }

    bool IsExpanded(uint64_t key) const {
        return options.expanded && options.expanded->count(key) > 0;
    }

    void SortChildren(std::vector<int>& list) const {
        std::stable_sort(list.begin(), list.end(), [this](int a, int b) {
            const Profiler::Node& na = frame.nodes[(size_t)a];
            const Profiler::Node& nb = frame.nodes[(size_t)b];
            if (na.totalNs != nb.totalNs) return na.totalNs > nb.totalNs;
            return Profiler::NameOf(na.nameId) < Profiler::NameOf(nb.nameId);
        });
    }

    void WindowStats(uint64_t key, bool synthetic, float& avgUs, float& maxUs) const {
        avgUs = 0.0f;
        maxUs = 0.0f;
        const size_t available = Profiler::FrameCount();
        const size_t window = std::min(options.historyWindow, available);
        if (window == 0) return;
        const Profiler::Track* track = synthetic ? nullptr : Profiler::FindTrack(key);
        if (!synthetic && !track) return;
        const bool gpuRow = key == kKeyGpu || (track && track->lane == Profiler::Lane::GPU);
        double sum = 0.0;
        size_t counted = 0;
        for (size_t age = 0; age < window; age++) {
            const Profiler::Frame* f = Profiler::FrameAt(age);
            if (!f) break;
            if (gpuRow && !f->gpuResolved) continue;
            float value = 0.0f;
            if (synthetic) {
                if (key == kKeyFrame) value = (float)((double)f->wallNs / 1000.0);
                else if (key == kKeyWork) value = (float)((double)f->workNs / 1000.0);
                else if (key == kKeyWait) value = (float)((double)f->waitNs / 1000.0);
                else if (key == kKeyGpu) value = (float)((double)f->gpuSpanNs / 1000.0);
            } else {
                float self = 0.0f;
                track->Sample(f->index, value, self);
            }
            sum += value;
            counted++;
            if (value > maxUs) maxUs = value;
        }
        avgUs = counted > 0 ? (float)(sum / (double)counted) : 0.0f;
    }

    void EmitNode(int index, int depth, std::vector<Row>& out) const {
        const Profiler::Node& node = frame.nodes[(size_t)index];
        Row row;
        row.key = node.key;
        row.parentKey = node.parent >= 0 ? frame.nodes[(size_t)node.parent].key : 0;
        row.name = Profiler::NameOf(node.nameId);
        row.depth = depth;
        row.lane = node.lane;
        row.isWait = (node.flags & Profiler::FlagWait) != 0;
        row.totalNs = node.totalNs;
        row.selfNs = node.selfNs;
        row.calls = node.calls;
        row.hasChildren = !children[(size_t)index].empty();
        row.expanded = row.hasChildren && IsExpanded(node.key);
        const int64_t denominator = node.lane == Profiler::Lane::GPU ? frame.gpuSpanNs : workNs;
        row.percentOfWork = (!row.isWait && denominator > 0) ? 100.0 * (double)node.totalNs / (double)denominator : -1.0;
        WindowStats(node.key, false, row.avgUs, row.maxUs);
        out.push_back(row);
        if (row.expanded) {
            for (int child : children[(size_t)index]) EmitNode(child, depth + 1, out);
        }
    }
};

Row MakeSynthetic(uint64_t key, uint64_t parentKey, const char* name, int depth, int64_t totalNs, bool isWait,
                  bool hasChildren, bool expanded) {
    Row row;
    row.key = key;
    row.parentKey = parentKey;
    row.name = name;
    row.depth = depth;
    row.synthetic = true;
    row.isWait = isWait;
    row.totalNs = totalNs;
    row.selfNs = 0;
    row.hasChildren = hasChildren;
    row.expanded = expanded;
    return row;
}

}

void BuildRows(const Profiler::Frame& frame, const BuildOptions& options, std::vector<Row>& out) {
    out.clear();

    Collector collector(frame, options);
    std::vector<int> workRoots;
    std::vector<int> waitRoots;
    std::vector<int> gpuRoots;
    int64_t topLevelWorkNs = 0;
    int64_t topLevelGpuNs = 0;
    for (size_t i = 0; i < frame.nodes.size(); i++) {
        const Profiler::Node& node = frame.nodes[i];
        if (node.parent >= 0) {
            collector.children[(size_t)node.parent].push_back((int)i);
            continue;
        }
        if (node.lane == Profiler::Lane::GPU) {
            gpuRoots.push_back((int)i);
            topLevelGpuNs += node.totalNs;
        } else if (node.flags & Profiler::FlagWait) {
            waitRoots.push_back((int)i);
        } else {
            workRoots.push_back((int)i);
            topLevelWorkNs += node.totalNs;
        }
    }
    for (std::vector<int>& list : collector.children) collector.SortChildren(list);
    collector.SortChildren(workRoots);
    collector.SortChildren(waitRoots);
    collector.SortChildren(gpuRoots);

    const bool showCpu = options.laneFilter != LaneFilter::GPU;
    const bool showGpu = options.laneFilter != LaneFilter::CPU && frame.gpuResolved;

    if (showCpu) {
        const int64_t unaccountedNs = std::max<int64_t>(0, frame.workNs - topLevelWorkNs);
        const bool frameExpanded = collector.IsExpanded(kKeyFrame);
        const bool workExpanded = collector.IsExpanded(kKeyWork);
        const bool waitExpanded = collector.IsExpanded(kKeyWait);

        Row frameRow = MakeSynthetic(kKeyFrame, 0, "Frame (wall clock)", 0, frame.wallNs, false, true, frameExpanded);
        collector.WindowStats(kKeyFrame, true, frameRow.avgUs, frameRow.maxUs);
        frameRow.calls = 1;
        out.push_back(frameRow);

        if (frameExpanded) {
            Row workRow = MakeSynthetic(kKeyWork, kKeyFrame, "Work (CPU busy)", 1, frame.workNs, false,
                                        !workRoots.empty() || unaccountedNs > 0, workExpanded);
            workRow.percentOfWork = frame.workNs > 0 ? 100.0 : -1.0;
            collector.WindowStats(kKeyWork, true, workRow.avgUs, workRow.maxUs);
            workRow.calls = 1;
            out.push_back(workRow);
            if (workExpanded) {
                for (int index : workRoots) collector.EmitNode(index, 2, out);
                if (unaccountedNs > 0) {
                    Row unaccounted = MakeSynthetic(kKeyUnaccounted, kKeyWork, "(unaccounted)", 2, unaccountedNs, false, false, false);
                    unaccounted.percentOfWork = frame.workNs > 0 ? 100.0 * (double)unaccountedNs / (double)frame.workNs : -1.0;
                    out.push_back(unaccounted);
                }
            }

            if (frame.waitNs > 0 || !waitRoots.empty()) {
                Row waitRow = MakeSynthetic(kKeyWait, kKeyFrame, "Wait (idle, not work)", 1, frame.waitNs, true,
                                            !waitRoots.empty(), waitExpanded);
                collector.WindowStats(kKeyWait, true, waitRow.avgUs, waitRow.maxUs);
                waitRow.calls = 1;
                out.push_back(waitRow);
                if (waitExpanded) {
                    for (int index : waitRoots) collector.EmitNode(index, 2, out);
                }
            }
        }
    }

    if (showGpu) {
        const int64_t idleNs = std::max<int64_t>(0, frame.gpuSpanNs - topLevelGpuNs);
        const bool gpuExpanded = collector.IsExpanded(kKeyGpu);
        Row gpuRow = MakeSynthetic(kKeyGpu, 0, "GPU (async timeline)", 0, frame.gpuSpanNs, false,
                                   !gpuRoots.empty() || idleNs > 0, gpuExpanded);
        gpuRow.lane = Profiler::Lane::GPU;
        gpuRow.percentOfWork = frame.gpuSpanNs > 0 ? 100.0 : -1.0;
        gpuRow.calls = 1;
        collector.WindowStats(kKeyGpu, true, gpuRow.avgUs, gpuRow.maxUs);
        out.push_back(gpuRow);
        if (gpuExpanded) {
            for (int index : gpuRoots) collector.EmitNode(index, 1, out);
            if (idleNs > 0) {
                Row idle = MakeSynthetic(kKeyGpuIdle, kKeyGpu, "(gpu idle / unattributed)", 1, idleNs, false, false, false);
                idle.lane = Profiler::Lane::GPU;
                idle.percentOfWork = frame.gpuSpanNs > 0 ? 100.0 * (double)idleNs / (double)frame.gpuSpanNs : -1.0;
                out.push_back(idle);
            }
        }
    }
}

void ExpandHottestPath(const Profiler::Frame& frame, std::unordered_set<uint64_t>& expanded) {
    expanded.insert(kKeyFrame);
    expanded.insert(kKeyWork);

    std::vector<std::vector<int>> children(frame.nodes.size());
    int bestCpu = -1;
    int bestGpu = -1;
    for (size_t i = 0; i < frame.nodes.size(); i++) {
        const Profiler::Node& node = frame.nodes[i];
        if (node.parent >= 0) {
            children[(size_t)node.parent].push_back((int)i);
        } else if (node.lane == Profiler::Lane::GPU) {
            if (bestGpu < 0 || node.totalNs > frame.nodes[(size_t)bestGpu].totalNs) bestGpu = (int)i;
        } else if (!(node.flags & Profiler::FlagWait)) {
            if (bestCpu < 0 || node.totalNs > frame.nodes[(size_t)bestCpu].totalNs) bestCpu = (int)i;
        }
    }

    auto descend = [&](int best) {
        while (best >= 0) {
            const std::vector<int>& list = children[(size_t)best];
            if (list.empty()) break;
            expanded.insert(frame.nodes[(size_t)best].key);
            int next = list[0];
            for (int candidate : list) {
                if (frame.nodes[(size_t)candidate].totalNs > frame.nodes[(size_t)next].totalNs) next = candidate;
            }
            best = next;
        }
    };
    descend(bestCpu);
    if (bestGpu >= 0) {
        expanded.insert(kKeyGpu);
        descend(bestGpu);
    }
}

std::vector<uint64_t> SelectLineKeys(const std::vector<Row>& rows, size_t topN,
                                     const std::unordered_set<uint64_t>& pinned,
                                     const std::unordered_set<uint64_t>& hidden) {
    std::vector<const Row*> candidates;
    for (const Row& row : rows) {
        if (row.synthetic || row.isWait || row.totalNs <= 0) continue;
        if (hidden.count(row.key)) continue;
        candidates.push_back(&row);
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const Row* a, const Row* b) {
        return a->totalNs > b->totalNs;
    });

    std::vector<uint64_t> keys;
    std::unordered_set<uint64_t> seen;
    for (size_t i = 0; i < candidates.size() && i < topN; i++) {
        keys.push_back(candidates[i]->key);
        seen.insert(candidates[i]->key);
    }
    std::vector<uint64_t> pinnedSorted(pinned.begin(), pinned.end());
    std::sort(pinnedSorted.begin(), pinnedSorted.end());
    for (uint64_t key : pinnedSorted) {
        if (!seen.count(key)) keys.push_back(key);
    }
    return keys;
}

void FillSeries(uint64_t key, size_t window, std::vector<float>& outUs) {
    outUs.assign(window, 0.0f);
    const bool synthetic = key == kKeyFrame || key == kKeyWork || key == kKeyWait || key == kKeyGpu;
    const Profiler::Track* track = synthetic ? nullptr : Profiler::FindTrack(key);
    const bool gpuSeries = key == kKeyGpu || (track && track->lane == Profiler::Lane::GPU);
    for (size_t i = 0; i < window; i++) {
        const size_t age = window - 1 - i;
        const Profiler::Frame* frame = Profiler::FrameAt(age);
        if (!frame) continue;
        if (gpuSeries && !frame->gpuResolved) {
            outUs[i] = std::numeric_limits<float>::quiet_NaN();
            continue;
        }
        float value = 0.0f;
        if (key == kKeyFrame) value = (float)((double)frame->wallNs / 1000.0);
        else if (key == kKeyWork) value = (float)((double)frame->workNs / 1000.0);
        else if (key == kKeyWait) value = (float)((double)frame->waitNs / 1000.0);
        else if (key == kKeyGpu) value = (float)((double)frame->gpuSpanNs / 1000.0);
        else if (track) {
            float self = 0.0f;
            track->Sample(frame->index, value, self);
        }
        outUs[i] = value;
    }
}

float NiceMaxUs(float valueUs) {
    if (valueUs <= 1.0f) return 1.0f;
    const float exponent = std::floor(std::log10(valueUs));
    const float base = std::pow(10.0f, exponent);
    const float mantissa = valueUs / base;
    float nice = 10.0f;
    if (mantissa <= 1.0f) nice = 1.0f;
    else if (mantissa <= 2.0f) nice = 2.0f;
    else if (mantissa <= 5.0f) nice = 5.0f;
    return nice * base;
}

size_t ColorAllocator::Get(uint64_t key) {
    auto it = assigned.find(key);
    if (it != assigned.end()) return it->second;
    const size_t index = next % paletteSize;
    next++;
    assigned.emplace(key, index);
    return index;
}

void ColorAllocator::Reset() {
    assigned.clear();
    next = 0;
}

}
