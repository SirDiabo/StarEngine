#pragma once

namespace PanelCachePolicy {

constexpr int kHoldFramesAfterActivity = 4;

struct Input {
    bool enabled = true;
    float refreshInterval = -1.0f;
    bool surfaceValid = false;
    bool geometryChanged = false;
    long long framesSinceShown = 0;
    int dirtyAllFrames = 0;
    bool mouseInside = false;
    bool mouseWasInside = false;
    bool focused = false;
    double secondsSinceRender = 0.0;
};

inline bool CanReuse(const Input& in) {
    if (!in.enabled || in.refreshInterval < 0.0f) return false;
    if (!in.surfaceValid || in.geometryChanged) return false;
    if (in.framesSinceShown != 1) return false;
    if (in.dirtyAllFrames > 0) return false;
    if (in.mouseInside || in.mouseWasInside) return false;
    if (in.focused) return false;
    if (in.secondsSinceRender >= (double)in.refreshInterval) return false;
    return true;
}

inline int NextDirtyAllFrames(int current, bool activity) {
    if (activity) return kHoldFramesAfterActivity;
    return current > 0 ? current - 1 : 0;
}

}
