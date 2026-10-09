#pragma once
#include "Profiler.h"
#include "ProfilerGpu.h"
#include "raylib.h"
#include "rlgl.h"

class ProfilerGpuScope {
public:
    explicit ProfilerGpuScope(uint32_t nameId)
        : active(PreFlush(nameId)), handle(active ? ProfilerGpu::BeginScope(nameId) : 0u) {}
    ~ProfilerGpuScope() {
        if (!active) return;
        rlDrawRenderBatchActive();
        ProfilerGpu::EndScope(handle);
    }
    ProfilerGpuScope(const ProfilerGpuScope&) = delete;
    ProfilerGpuScope& operator=(const ProfilerGpuScope&) = delete;

private:
    static bool PreFlush(uint32_t nameId) {
        const bool enabled = Profiler::IsEnabled() && nameId != 0;
        if (enabled) rlDrawRenderBatchActive();
        return enabled;
    }

    bool active;
    uint32_t handle;
};

class ProfilerRenderScope {
public:
    explicit ProfilerRenderScope(uint32_t nameId)
        : active(PreFlush(nameId)), scope(active ? nameId : 0u), handle(active ? ProfilerGpu::BeginScope(nameId) : 0u) {}
    ~ProfilerRenderScope() {
        if (!active) return;
        rlDrawRenderBatchActive();
        ProfilerGpu::EndScope(handle);
    }
    ProfilerRenderScope(const ProfilerRenderScope&) = delete;
    ProfilerRenderScope& operator=(const ProfilerRenderScope&) = delete;

private:
    static bool PreFlush(uint32_t nameId) {
        const bool enabled = Profiler::IsEnabled() && nameId != 0;
        if (enabled) rlDrawRenderBatchActive();
        return enabled;
    }

    bool active;
    Profiler::Scope scope;
    uint32_t handle;
};

#define PROFILE_RENDER_SCOPE(name) \
    static const uint32_t PROFILER_CONCAT(profilerId_, __LINE__) = Profiler::Intern(name); \
    ProfilerRenderScope PROFILER_CONCAT(profilerScope_, __LINE__)(PROFILER_CONCAT(profilerId_, __LINE__))

#define PROFILE_RENDER_SCOPE_DYN(labelExpr) \
    ProfilerRenderScope PROFILER_CONCAT(profilerScope_, __LINE__)( \
        Profiler::IsEnabled() ? Profiler::Intern(labelExpr) : 0u)

#define PROFILE_GPU_SCOPE(name) \
    static const uint32_t PROFILER_CONCAT(profilerId_, __LINE__) = Profiler::Intern(name); \
    ProfilerGpuScope PROFILER_CONCAT(profilerScope_, __LINE__)(PROFILER_CONCAT(profilerId_, __LINE__))

#define PROFILE_GPU_SCOPE_DYN(labelExpr) \
    ProfilerGpuScope PROFILER_CONCAT(profilerScope_, __LINE__)( \
        Profiler::IsEnabled() ? Profiler::Intern(labelExpr) : 0u)

#define PROFILE_GPU_SCOPE_DYN_L(level, labelExpr) \
    ProfilerGpuScope PROFILER_CONCAT(profilerScope_, __LINE__)( \
        Profiler::DetailEnabled(level) ? Profiler::Intern(labelExpr) : 0u)
