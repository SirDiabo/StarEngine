#pragma once
#include "Profiler.h"
#include "raylib.h"
#include "rlgl.h"

class ProfilerRenderScope {
public:
    explicit ProfilerRenderScope(uint32_t nameId)
        : scope(nameId), flushOnExit(Profiler::IsEnabled() && nameId != 0) {}
    ~ProfilerRenderScope() {
        if (flushOnExit) rlDrawRenderBatchActive();
    }
    ProfilerRenderScope(const ProfilerRenderScope&) = delete;
    ProfilerRenderScope& operator=(const ProfilerRenderScope&) = delete;

private:
    Profiler::Scope scope;
    bool flushOnExit;
};

#define PROFILE_RENDER_SCOPE(name) \
    static const uint32_t PROFILER_CONCAT(profilerId_, __LINE__) = Profiler::Intern(name); \
    ProfilerRenderScope PROFILER_CONCAT(profilerScope_, __LINE__)(PROFILER_CONCAT(profilerId_, __LINE__))
