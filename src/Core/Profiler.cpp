#include "Profiler.h"
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

namespace Profiler {

namespace Internal {
bool enabled = false;
int detail = DetailStandard;
}

namespace {

struct OpenScope {
    uint64_t key;
    int32_t node;
    int64_t start;
    int64_t childNs;
};

size_t g_history = kHistoryFrames;

size_t RingSize() { return g_history + 1; }

std::mutex g_internMutex;
std::unordered_map<std::string, uint32_t> g_internIds;
std::vector<std::string> g_internNames = { std::string() };

std::vector<Frame> g_ring;
size_t g_head = 0;
size_t g_count = 0;
Frame* g_current = nullptr;
std::vector<OpenScope> g_stack;
std::unordered_map<uint64_t, int32_t> g_nodeIndex;
std::unordered_map<uint64_t, Track> g_tracks;

std::thread::id g_mainThread;
bool g_mainThreadSet = false;
uint64_t g_frameCounter = 0;
uint64_t g_lastRequestFrame = 0;
bool g_requested = false;
bool g_forced = false;
bool g_captureRecording = false;
bool g_autoPlay = false;
bool g_playing = false;
bool g_playRecording = false;
bool g_gpuAvailable = false;
int64_t g_frameStartNs = 0;
double g_overheadNs = -1.0;

uint64_t Mix(uint64_t x) {
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return x;
}

void EnsureRing() {
    if (g_ring.size() != RingSize()) g_ring.assign(RingSize(), Frame());
}

void UpdateTrack(const Frame& frame, size_t nodeIndex, size_t slot) {
    const Node& node = frame.nodes[nodeIndex];
    auto it = g_tracks.find(node.key);
    if (it == g_tracks.end()) {
        Track track;
        track.key = node.key;
        track.nameId = node.nameId;
        track.lane = node.lane;
        track.flags = node.flags;
        track.totalUs.assign(g_history, 0.0f);
        track.selfUs.assign(g_history, 0.0f);
        track.stamp.assign(g_history, 0u);
        it = g_tracks.emplace(node.key, std::move(track)).first;
    }
    Track& track = it->second;
    if (node.parent >= 0) {
        const Node& parentNode = frame.nodes[(size_t)node.parent];
        track.parentKey = parentNode.key;
        auto parentIt = g_tracks.find(parentNode.key);
        track.depth = parentIt != g_tracks.end() ? parentIt->second.depth + 1 : 1;
    } else {
        track.parentKey = 0;
        track.depth = 0;
    }
    track.totalUs[slot] = (float)((double)node.totalNs / 1000.0);
    track.selfUs[slot] = (float)((double)node.selfNs / 1000.0);
    track.stamp[slot] = (uint32_t)frame.index;
    track.lastSeenFrame = frame.index;
}

void FinalizeFrame(int64_t nowNs) {
    Frame& frame = *g_current;
    frame.wallNs = nowNs - g_frameStartNs;

    int64_t waitNs = 0;
    uint64_t calls = 0;
    for (const Node& node : frame.nodes) {
        calls += node.calls;
        if (node.parent < 0 && (node.flags & FlagWait)) waitNs += node.totalNs;
    }
    frame.waitNs = waitNs;
    frame.workNs = frame.wallNs - waitNs;
    frame.totalCalls = calls;

    const size_t slot = (size_t)(frame.index % g_history);
    for (size_t i = 0; i < frame.nodes.size(); i++) UpdateTrack(frame, i, slot);

    g_head = (g_head + 1) % RingSize();
    if (g_count < g_history) g_count++;

    if (frame.index % 120 == 0) {
        for (auto it = g_tracks.begin(); it != g_tracks.end();) {
            if (it->second.lastSeenFrame + g_history < frame.index) it = g_tracks.erase(it);
            else ++it;
        }
    }

    g_current = nullptr;
}

void Calibrate() {
    const uint32_t id = Intern("Profiler Calibration");
    Frame scratch;
    Frame* savedCurrent = g_current;
    g_current = &scratch;
    g_stack.clear();
    g_nodeIndex.clear();
    const int kIterations = 4000;
    const int64_t start = NowNs();
    for (int i = 0; i < kIterations; i++) {
        Internal::Begin(id, Lane::CPU, FlagNone);
        Internal::End();
    }
    const int64_t elapsed = NowNs() - start;
    g_overheadNs = (double)elapsed / (double)kIterations;
    g_stack.clear();
    g_nodeIndex.clear();
    g_current = savedCurrent;
}

}

bool Track::Sample(uint64_t frameIndex, float& outTotalUs, float& outSelfUs) const {
    if (stamp.empty()) {
        outTotalUs = 0.0f;
        outSelfUs = 0.0f;
        return false;
    }
    const size_t slot = (size_t)(frameIndex % stamp.size());
    if (stamp[slot] != (uint32_t)frameIndex) {
        outTotalUs = 0.0f;
        outSelfUs = 0.0f;
        return false;
    }
    outTotalUs = totalUs[slot];
    outSelfUs = selfUs[slot];
    return true;
}

int64_t NowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

uint32_t Intern(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_internMutex);
    auto it = g_internIds.find(name);
    if (it != g_internIds.end()) return it->second;
    uint32_t id = (uint32_t)g_internNames.size();
    g_internNames.push_back(name);
    g_internIds.emplace(name, id);
    return id;
}

uint32_t Intern(const char* name) {
    return Intern(std::string(name ? name : ""));
}

const std::string& NameOf(uint32_t id) {
    std::lock_guard<std::mutex> lock(g_internMutex);
    static const std::string unknown = "?";
    if (id >= g_internNames.size()) return unknown;
    return g_internNames[id];
}

uint64_t ChildKey(uint64_t parentKey, uint32_t nameId, Lane lane) {
    return Mix(parentKey ^ (Mix((uint64_t)nameId) + ((uint64_t)lane << 40) + 0x9E3779B97F4A7C15ull));
}

namespace Internal {

bool Begin(uint32_t nameId, Lane lane, uint8_t flags) {
    if (!g_current) return false;
    if (std::this_thread::get_id() != g_mainThread) return false;

    const uint64_t parentKey = g_stack.empty() ? kRootKey : g_stack.back().key;
    const uint64_t key = ChildKey(parentKey, nameId, lane);

    int32_t nodeIndex;
    auto it = g_nodeIndex.find(key);
    if (it == g_nodeIndex.end()) {
        Node node;
        node.key = key;
        node.nameId = nameId;
        node.parent = g_stack.empty() ? -1 : g_stack.back().node;
        node.lane = lane;
        node.flags = flags;
        nodeIndex = (int32_t)g_current->nodes.size();
        g_current->nodes.push_back(node);
        g_nodeIndex.emplace(key, nodeIndex);
    } else {
        nodeIndex = it->second;
    }

    g_stack.push_back({ key, nodeIndex, 0, 0 });
    g_stack.back().start = NowNs();
    return true;
}

void End() {
    const int64_t end = NowNs();
    if (g_stack.empty() || !g_current) return;
    OpenScope open = g_stack.back();
    g_stack.pop_back();
    const int64_t elapsed = end - open.start;

    Node& node = g_current->nodes[(size_t)open.node];
    node.totalNs += elapsed;
    node.selfNs += elapsed - open.childNs;
    node.calls++;

    if (!g_stack.empty()) g_stack.back().childNs += elapsed;
}

}

void RequestRecording() {
    g_requested = true;
    g_lastRequestFrame = g_frameCounter;
}

void SetForcedRecording(bool forced) { g_forced = forced; }
void SetCaptureRecording(bool active) { g_captureRecording = active; }
bool IsCaptureRecording() { return g_captureRecording; }
size_t HistoryFrames() { return g_history; }

void SetHistoryFrames(size_t frames) {
    if (frames < kMinHistoryFrames) frames = kMinHistoryFrames;
    if (frames > kMaxHistoryFrames) frames = kMaxHistoryFrames;
    Clear();
    g_history = frames;
    g_ring.assign(RingSize(), Frame());
}

void InjectFrame(Frame&& frame) {
    if (g_current) return;
    EnsureRing();
    Frame& target = g_ring[g_head];
    target = std::move(frame);
    uint64_t calls = 0;
    for (const Node& node : target.nodes) calls += node.calls;
    target.totalCalls = calls;
    const size_t slot = (size_t)(target.index % g_history);
    for (size_t i = 0; i < target.nodes.size(); i++) UpdateTrack(target, i, slot);
    g_head = (g_head + 1) % RingSize();
    if (g_count < g_history) g_count++;
    if (target.index % 120 == 0) {
        for (auto it = g_tracks.begin(); it != g_tracks.end();) {
            if (it->second.lastSeenFrame + g_history < target.index) it = g_tracks.erase(it);
            else ++it;
        }
    }
}
bool IsForcedRecording() { return g_forced; }
void SetAutoRecordOnPlay(bool enabled) { g_autoPlay = enabled; }
bool IsAutoRecordOnPlay() { return g_autoPlay; }
void NotifyPlayState(bool playing) { g_playing = playing; }
bool IsPlayRecordingActive() { return g_playRecording; }
void SetDetail(int level) { Internal::detail = level < 0 ? 0 : (level > DetailVerbose ? DetailVerbose : level); }
int GetDetail() { return Internal::detail; }

void BeginFrame() {
    EnsureRing();
    const int64_t now = NowNs();
    if (!g_mainThreadSet) {
        g_mainThread = std::this_thread::get_id();
        g_mainThreadSet = true;
    }

    if (g_current) {
        g_stack.clear();
        FinalizeFrame(now);
    }

    g_frameCounter++;

    if (g_autoPlay) {
        if (g_playing && !g_playRecording) {
            Clear();
            g_playRecording = true;
        } else if (!g_playing && g_playRecording) {
            g_playRecording = false;
        }
    } else {
        g_playRecording = false;
    }

    const bool requestActive = !g_autoPlay && g_requested && g_frameCounter - g_lastRequestFrame <= 2;
    const bool shouldRecord = g_forced || g_captureRecording || g_playRecording || requestActive;
    Internal::enabled = shouldRecord;
    if (!shouldRecord) return;

    if (g_overheadNs < 0.0) Calibrate();

    g_current = &g_ring[g_head];
    g_current->nodes.clear();
    g_current->index = g_frameCounter;
    g_current->wallNs = 0;
    g_current->workNs = 0;
    g_current->waitNs = 0;
    g_current->gpuSpanNs = 0;
    g_current->gpuResolved = false;
    g_current->totalCalls = 0;
    g_stack.clear();
    g_nodeIndex.clear();
    g_frameStartNs = NowNs();
}

void Clear() {
    g_current = nullptr;
    g_stack.clear();
    g_nodeIndex.clear();
    g_tracks.clear();
    g_head = 0;
    g_count = 0;
}

uint64_t CurrentFrameIndex() { return g_frameCounter; }
void SetGpuAvailable(bool available) { g_gpuAvailable = available; }
bool IsGpuAvailable() { return g_gpuAvailable; }

bool ResolveGpuFrame(uint64_t frameIndex, int64_t spanNs, const std::vector<GpuScopeResult>& scopes) {
    Frame* frame = nullptr;
    for (size_t age = 0; age < g_count; age++) {
        Frame* candidate = &g_ring[(g_head + RingSize() - 1 - age) % RingSize()];
        if (candidate->index == frameIndex) { frame = candidate; break; }
        if (candidate->index < frameIndex) break;
    }
    if (!frame || frame->gpuResolved) return false;

    const size_t firstGpuNode = frame->nodes.size();
    std::vector<int32_t> nodeOfScope(scopes.size(), -1);
    std::unordered_map<uint64_t, int32_t> localIndex;
    for (size_t i = 0; i < scopes.size(); i++) {
        const GpuScopeResult& result = scopes[i];
        const int32_t parentNode = (result.parent >= 0 && (size_t)result.parent < i) ? nodeOfScope[(size_t)result.parent] : -1;
        const uint64_t parentKey = parentNode >= 0 ? frame->nodes[(size_t)parentNode].key : kRootKey;
        const uint64_t key = ChildKey(parentKey, result.nameId, Lane::GPU);
        int32_t nodeIndex;
        auto it = localIndex.find(key);
        if (it == localIndex.end()) {
            Node node;
            node.key = key;
            node.nameId = result.nameId;
            node.parent = parentNode;
            node.lane = Lane::GPU;
            nodeIndex = (int32_t)frame->nodes.size();
            frame->nodes.push_back(node);
            localIndex.emplace(key, nodeIndex);
        } else {
            nodeIndex = it->second;
        }
        Node& node = frame->nodes[(size_t)nodeIndex];
        node.totalNs += result.ns < 0 ? 0 : result.ns;
        node.calls++;
        nodeOfScope[i] = nodeIndex;
    }

    for (size_t i = firstGpuNode; i < frame->nodes.size(); i++) frame->nodes[i].selfNs = frame->nodes[i].totalNs;
    for (size_t i = firstGpuNode; i < frame->nodes.size(); i++) {
        const int32_t parent = frame->nodes[i].parent;
        if (parent >= 0) {
            Node& parentNode = frame->nodes[(size_t)parent];
            parentNode.selfNs -= frame->nodes[i].totalNs;
            if (parentNode.selfNs < 0) parentNode.selfNs = 0;
        }
    }

    frame->gpuSpanNs = spanNs < 0 ? 0 : spanNs;
    frame->gpuResolved = true;
    const size_t slot = (size_t)(frame->index % g_history);
    for (size_t i = firstGpuNode; i < frame->nodes.size(); i++) UpdateTrack(*frame, i, slot);
    return true;
}

size_t DisplayAge() {
    if (!g_gpuAvailable) return 0;
    const size_t limit = g_count < 16 ? g_count : 16;
    for (size_t age = 0; age < limit; age++) {
        const Frame* frame = FrameAt(age);
        if (frame && frame->gpuResolved) return age;
    }
    return 0;
}

size_t FrameCount() { return g_count; }

const Frame* FrameAt(size_t ageFromNewest) {
    if (ageFromNewest >= g_count || g_ring.empty()) return nullptr;
    return &g_ring[(g_head + RingSize() - 1 - ageFromNewest) % RingSize()];
}

const Track* FindTrack(uint64_t key) {
    auto it = g_tracks.find(key);
    return it != g_tracks.end() ? &it->second : nullptr;
}

const std::unordered_map<uint64_t, Track>& Tracks() { return g_tracks; }

double OverheadNsPerScope() { return g_overheadNs < 0.0 ? 0.0 : g_overheadNs; }

std::string FormatDuration(int64_t ns) {
    if (ns < 0) return "-" + FormatDuration(-ns);
    char buf[48];
    if (ns < 1000) {
        snprintf(buf, sizeof(buf), "%lldns", (long long)ns);
    } else if (ns < 999500) {
        double us = (double)ns / 1000.0;
        if (us >= 100.0) snprintf(buf, sizeof(buf), "%.0f\xC2\xB5s", us);
        else if (us >= 10.0) snprintf(buf, sizeof(buf), "%.1f\xC2\xB5s", us);
        else snprintf(buf, sizeof(buf), "%.2f\xC2\xB5s", us);
    } else if (ns < 1000000000) {
        double ms = (double)ns / 1000000.0;
        if (ms >= 99.995) snprintf(buf, sizeof(buf), "%.1fms", ms);
        else snprintf(buf, sizeof(buf), "%.2fms", ms);
    } else {
        snprintf(buf, sizeof(buf), "%.2fs", (double)ns / 1000000000.0);
    }
    return buf;
}

}
