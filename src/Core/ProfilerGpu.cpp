#include "ProfilerGpu.h"
#include "Profiler.h"
#include "raylib.h"
#include "rlgl.h"
#include <deque>
#include <vector>

namespace ProfilerGpu {

namespace {

constexpr size_t kMaxScopesPerFrame = 1024;
constexpr int kMaxFrameAge = 16;
constexpr int kQueryChunk = 64;

struct Scope {
    uint32_t nameId = 0;
    int32_t parent = -1;
    unsigned begin = 0;
    unsigned end = 0;
};

struct FrameRecord {
    uint64_t frameIndex = 0;
    unsigned spanBegin = 0;
    unsigned spanEnd = 0;
    bool spanEndSet = false;
    int age = 0;
    std::vector<Scope> scopes;
};

Backend g_backend;
bool g_initialized = false;
bool g_available = false;
bool g_backendOverridden = false;
Backend g_overrideBackend;

bool g_recording = false;
FrameRecord g_current;
std::vector<int32_t> g_stack;
std::deque<FrameRecord> g_pending;
std::vector<unsigned> g_freeQueries;

bool BackendComplete(const Backend& b) {
    return b.genQueries && b.deleteQueries && b.queryCounter && b.getQueryObjectiv && b.getQueryObjectui64v;
}

void EnsureInitialized() {
    if (g_initialized) return;
    g_initialized = true;

    if (g_backendOverridden) {
        g_backend = g_overrideBackend;
        g_available = BackendComplete(g_backend);
        Profiler::SetGpuAvailable(g_available);
        return;
    }

    const int version = rlGetVersion();
    if (version != RL_OPENGL_33 && version != RL_OPENGL_43) return;

    Backend loaded;
    loaded.genQueries = (decltype(loaded.genQueries))rlGetProcAddress("glGenQueries");
    loaded.deleteQueries = (decltype(loaded.deleteQueries))rlGetProcAddress("glDeleteQueries");
    loaded.queryCounter = (decltype(loaded.queryCounter))rlGetProcAddress("glQueryCounter");
    loaded.getQueryObjectiv = (decltype(loaded.getQueryObjectiv))rlGetProcAddress("glGetQueryObjectiv");
    loaded.getQueryObjectui64v = (decltype(loaded.getQueryObjectui64v))rlGetProcAddress("glGetQueryObjectui64v");
    if (!BackendComplete(loaded)) return;

    g_backend = loaded;
    g_available = true;
    Profiler::SetGpuAvailable(true);
}

unsigned AcquireQuery() {
    if (g_freeQueries.empty()) {
        unsigned ids[kQueryChunk];
        g_backend.genQueries(kQueryChunk, ids);
        for (int i = 0; i < kQueryChunk; i++) g_freeQueries.push_back(ids[i]);
    }
    unsigned id = g_freeQueries.back();
    g_freeQueries.pop_back();
    return id;
}

void Timestamp(unsigned id) {
    g_backend.queryCounter(id, kGlTimestamp);
}

void ReleaseRecord(FrameRecord& record, bool deleteQueries) {
    std::vector<unsigned> ids;
    ids.push_back(record.spanBegin);
    if (record.spanEndSet) ids.push_back(record.spanEnd);
    for (const Scope& scope : record.scopes) {
        ids.push_back(scope.begin);
        if (scope.end) ids.push_back(scope.end);
    }
    if (deleteQueries) {
        g_backend.deleteQueries((int)ids.size(), ids.data());
    } else {
        for (unsigned id : ids) g_freeQueries.push_back(id);
    }
}

uint64_t ReadResult(unsigned id) {
    uint64_t value = 0;
    g_backend.getQueryObjectui64v(id, kGlQueryResult, &value);
    return value;
}

void ResolvePending() {
    while (!g_pending.empty()) {
        FrameRecord& record = g_pending.front();
        record.age++;

        int available = 0;
        g_backend.getQueryObjectiv(record.spanEnd, kGlQueryResultAvailable, &available);
        if (!available) {
            if (record.age > kMaxFrameAge) {
                ReleaseRecord(record, true);
                g_pending.pop_front();
                continue;
            }
            break;
        }

        std::vector<Profiler::GpuScopeResult> results;
        results.reserve(record.scopes.size());
        for (const Scope& scope : record.scopes) {
            Profiler::GpuScopeResult result;
            result.nameId = scope.nameId;
            result.parent = scope.parent;
            if (scope.end) {
                const uint64_t begin = ReadResult(scope.begin);
                const uint64_t end = ReadResult(scope.end);
                result.ns = end > begin ? (int64_t)(end - begin) : 0;
            }
            results.push_back(result);
        }
        const uint64_t spanBegin = ReadResult(record.spanBegin);
        const uint64_t spanEnd = ReadResult(record.spanEnd);
        const int64_t span = spanEnd > spanBegin ? (int64_t)(spanEnd - spanBegin) : 0;

        Profiler::ResolveGpuFrame(record.frameIndex, span, results);
        ReleaseRecord(record, false);
        g_pending.pop_front();
    }
}

void CloseCurrentFrame() {
    FrameEnd();
    for (Scope& scope : g_current.scopes) {
        if (!scope.end) {
            scope.end = AcquireQuery();
            Timestamp(scope.end);
        }
    }
    g_pending.push_back(std::move(g_current));
    g_current = FrameRecord();
    g_stack.clear();
    g_recording = false;
}

void OpenFrame() {
    g_current = FrameRecord();
    g_current.frameIndex = Profiler::CurrentFrameIndex();
    g_current.spanBegin = AcquireQuery();
    Timestamp(g_current.spanBegin);
    g_stack.clear();
    g_recording = true;
}

}

void SetBackend(const Backend* backend) {
    g_pending.clear();
    g_freeQueries.clear();
    g_current = FrameRecord();
    g_stack.clear();
    g_recording = false;
    g_initialized = false;
    g_available = false;
    g_backendOverridden = backend != nullptr;
    if (backend) g_overrideBackend = *backend;
    Profiler::SetGpuAvailable(false);
}

bool Available() {
    EnsureInitialized();
    return g_available;
}

void Pump() {
    EnsureInitialized();
    if (!g_available) return;
    if (g_recording) CloseCurrentFrame();
    ResolvePending();
    if (Profiler::IsEnabled()) OpenFrame();
}

void DiscardPending() {
    if (!g_available) return;
    if (g_recording) {
        ReleaseRecord(g_current, true);
        g_current = FrameRecord();
        g_stack.clear();
        g_recording = false;
    }
    for (FrameRecord& record : g_pending) ReleaseRecord(record, true);
    g_pending.clear();
}

void FrameEnd() {
    if (!g_available || !g_recording || g_current.spanEndSet) return;
    g_current.spanEnd = AcquireQuery();
    Timestamp(g_current.spanEnd);
    g_current.spanEndSet = true;
}

uint32_t BeginScope(uint32_t nameId) {
    if (!g_available || !g_recording || nameId == 0) return 0;
    if (g_current.scopes.size() >= kMaxScopesPerFrame) return 0;
    Scope scope;
    scope.nameId = nameId;
    scope.parent = g_stack.empty() ? -1 : g_stack.back();
    scope.begin = AcquireQuery();
    Timestamp(scope.begin);
    g_current.scopes.push_back(scope);
    g_stack.push_back((int32_t)g_current.scopes.size() - 1);
    return (uint32_t)g_current.scopes.size();
}

void EndScope(uint32_t handle) {
    if (handle == 0 || !g_available || !g_recording) return;
    const size_t index = (size_t)handle - 1;
    if (index >= g_current.scopes.size() || g_stack.empty() || g_stack.back() != (int32_t)index) return;
    g_current.scopes[index].end = AcquireQuery();
    Timestamp(g_current.scopes[index].end);
    g_stack.pop_back();
}

size_t PendingFrameCount() { return g_pending.size(); }
size_t FreeQueryCount() { return g_freeQueries.size(); }

}
