#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Profiler {

constexpr size_t kHistoryFrames = 600;
constexpr size_t kMaxHistoryFrames = 3600;
constexpr size_t kMinHistoryFrames = 60;
constexpr uint64_t kRootKey = 0x9E3779B97F4A7C15ull;

enum class Lane : uint8_t { CPU = 0, GPU = 1 };

enum : uint8_t { FlagNone = 0, FlagWait = 1 };

enum DetailLevel : int { DetailStandard = 0, DetailNodes = 1, DetailVerbose = 2 };

struct Node {
    uint64_t key = 0;
    uint32_t nameId = 0;
    int32_t parent = -1;
    Lane lane = Lane::CPU;
    uint8_t flags = FlagNone;
    uint32_t calls = 0;
    int64_t totalNs = 0;
    int64_t selfNs = 0;
};

struct Frame {
    uint64_t index = 0;
    int64_t wallNs = 0;
    int64_t workNs = 0;
    int64_t waitNs = 0;
    int64_t gpuSpanNs = 0;
    bool gpuResolved = false;
    uint64_t totalCalls = 0;
    std::vector<Node> nodes;
};

struct GpuScopeResult {
    uint32_t nameId = 0;
    int32_t parent = -1;
    int64_t ns = 0;
};

struct Track {
    uint64_t key = 0;
    uint64_t parentKey = 0;
    uint32_t nameId = 0;
    Lane lane = Lane::CPU;
    uint8_t flags = FlagNone;
    int depth = 0;
    uint64_t lastSeenFrame = 0;
    std::vector<float> totalUs;
    std::vector<float> selfUs;
    std::vector<uint32_t> stamp;

    bool Sample(uint64_t frameIndex, float& outTotalUs, float& outSelfUs) const;
};

namespace Internal {
extern bool enabled;
extern int detail;
bool Begin(uint32_t nameId, Lane lane, uint8_t flags);
void End();
}

inline bool IsEnabled() { return Internal::enabled; }
inline bool DetailEnabled(int level) { return Internal::enabled && Internal::detail >= level; }

uint32_t Intern(const char* name);
uint32_t Intern(const std::string& name);
const std::string& NameOf(uint32_t id);
uint64_t ChildKey(uint64_t parentKey, uint32_t nameId, Lane lane);

void RequestRecording();
void SetForcedRecording(bool forced);
void SetCaptureRecording(bool active);
bool IsCaptureRecording();
size_t HistoryFrames();
void SetHistoryFrames(size_t frames);
void InjectFrame(Frame&& frame);
bool IsForcedRecording();
void SetAutoRecordOnPlay(bool enabled);
bool IsAutoRecordOnPlay();
void NotifyPlayState(bool playing);
bool IsPlayRecordingActive();
void SetDetail(int level);
int GetDetail();
void BeginFrame();
void Clear();
uint64_t CurrentFrameIndex();
void SetGpuAvailable(bool available);
bool IsGpuAvailable();
bool ResolveGpuFrame(uint64_t frameIndex, int64_t spanNs, const std::vector<GpuScopeResult>& scopes);
size_t DisplayAge();

size_t FrameCount();
const Frame* FrameAt(size_t ageFromNewest);
const Track* FindTrack(uint64_t key);
const std::unordered_map<uint64_t, Track>& Tracks();

int64_t NowNs();
double OverheadNsPerScope();
std::string FormatDuration(int64_t ns);

class Scope {
public:
    explicit Scope(uint32_t nameId, Lane lane = Lane::CPU, uint8_t flags = FlagNone) {
        if (Internal::enabled && nameId != 0) active = Internal::Begin(nameId, lane, flags);
    }
    ~Scope() {
        if (active) Internal::End();
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    bool active = false;
};

}

#define PROFILER_CONCAT_INNER(a, b) a##b
#define PROFILER_CONCAT(a, b) PROFILER_CONCAT_INNER(a, b)

#define PROFILE_SCOPE(name) \
    static const uint32_t PROFILER_CONCAT(profilerId_, __LINE__) = Profiler::Intern(name); \
    Profiler::Scope PROFILER_CONCAT(profilerScope_, __LINE__)(PROFILER_CONCAT(profilerId_, __LINE__))

#define PROFILE_SCOPE_L(level, name) \
    static const uint32_t PROFILER_CONCAT(profilerId_, __LINE__) = Profiler::Intern(name); \
    Profiler::Scope PROFILER_CONCAT(profilerScope_, __LINE__)( \
        Profiler::DetailEnabled(level) ? PROFILER_CONCAT(profilerId_, __LINE__) : 0u)

#define PROFILE_WAIT_SCOPE(name) \
    static const uint32_t PROFILER_CONCAT(profilerId_, __LINE__) = Profiler::Intern(name); \
    Profiler::Scope PROFILER_CONCAT(profilerScope_, __LINE__)( \
        PROFILER_CONCAT(profilerId_, __LINE__), Profiler::Lane::CPU, Profiler::FlagWait)

#define PROFILE_SCOPE_DYN(labelExpr) \
    Profiler::Scope PROFILER_CONCAT(profilerScope_, __LINE__)( \
        Profiler::IsEnabled() ? Profiler::Intern(labelExpr) : 0u)

#define PROFILE_SCOPE_DYN_L(level, labelExpr) \
    Profiler::Scope PROFILER_CONCAT(profilerScope_, __LINE__)( \
        Profiler::DetailEnabled(level) ? Profiler::Intern(labelExpr) : 0u)
