#include "PanelCache.h"
#include "PanelCachePolicy.h"
#include "Core/Panel.h"
#include "Core/Widgets/WidgetInput.h"
#include "rlgl.h"
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace {

struct Entry {
    PanelCache::Surface surface;
    long long lastShownFrame = -1000;
    double lastRenderTime = 0.0;
    bool mouseWasInside = false;
};

struct IntRect {
    int x;
    int y;
    int width;
    int height;
};

std::unordered_map<const Panel*, Entry> entries;
long long frameIndex = 0;
int dirtyAllFrames = 0;

bool DisabledByEnvironment() {
#ifdef _MSC_VER
#pragma warning(suppress : 4996)
#endif
    static const bool disabled = std::getenv("STARENGINE_NO_PANEL_CACHE") != nullptr;
    return disabled;
}

bool AnyKeyActive() {
    for (int key = 32; key <= 348; key++) {
        if (IsKeyDown(key) || IsKeyPressed(key) || IsKeyReleased(key)) return true;
    }
    return false;
}

IntRect ToIntRect(Rectangle area) {
    int x = (int)std::floor(area.x);
    int y = (int)std::floor(area.y);
    int x1 = (int)std::ceil(area.x + area.width);
    int y1 = (int)std::ceil(area.y + area.height);
    return { x, y, x1 - x, y1 - y };
}

}

namespace PanelCache {

bool CaptureScreenRect(Surface& surface, Rectangle area) {
    if (GetScreenWidth() != GetRenderWidth() || GetScreenHeight() != GetRenderHeight()) return false;
    if (rlGetActiveFramebuffer() != 0) return false;

    IntRect r = ToIntRect(area);
    int framebufferWidth = GetRenderWidth();
    int framebufferHeight = GetRenderHeight();
    if (r.x < 0 || r.y < 0 || r.width <= 0 || r.height <= 0) return false;
    if (r.x + r.width > framebufferWidth || r.y + r.height > framebufferHeight) return false;

    if (surface.target.id == 0 || surface.width != r.width || surface.height != r.height) {
        if (surface.target.id != 0) UnloadRenderTexture(surface.target);
        surface.target = LoadRenderTexture(r.width, r.height);
        surface.width = r.width;
        surface.height = r.height;
        if (surface.target.id == 0) return false;
        SetTextureFilter(surface.target.texture, TEXTURE_FILTER_POINT);
    }

    rlDrawRenderBatchActive();
    rlBindFramebuffer(RL_READ_FRAMEBUFFER, 0);
    rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, surface.target.id);
    rlBlitFramebuffer(r.x, framebufferHeight - (r.y + r.height), r.x + r.width, framebufferHeight - r.y,
                      0, 0, r.width, r.height, 0x00004000);
    rlBindFramebuffer(RL_READ_FRAMEBUFFER, 0);
    rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, 0);

    surface.x = r.x;
    surface.y = r.y;
    surface.valid = true;
    return true;
}

void DrawSurface(const Surface& surface) {
    Rectangle source = { 0.0f, 0.0f, (float)surface.width, -(float)surface.height };
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    DrawTextureRec(surface.target.texture, source, { (float)surface.x, (float)surface.y }, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
}

void BeginFrame() {
    frameIndex++;

    Vector2 wheel = GetMouseWheelMoveV();
    bool activity = false;
    for (int button = MOUSE_BUTTON_LEFT; button <= MOUSE_BUTTON_MIDDLE && !activity; button++) {
        activity = IsMouseButtonDown(button) || IsMouseButtonPressed(button) || IsMouseButtonReleased(button);
    }
    activity = activity || wheel.x != 0.0f || wheel.y != 0.0f || IsWindowResized() || IsFileDropped()
        || WidgetInput::IsMouseBlocked() || WidgetInput::HasModal(WidgetInput::kEditorDevice) || AnyKeyActive();

    dirtyAllFrames = PanelCachePolicy::NextDirtyAllFrames(dirtyAllFrames, activity);
}

bool TryDraw(Panel* panel) {
    float interval = panel->CacheRefreshInterval();
    if (interval < 0.0f || DisabledByEnvironment()) return false;

    Entry& entry = entries[panel];
    bool inside = CheckCollisionPointRec(GetMousePosition(), panel->bounds);
    bool wasInside = entry.mouseWasInside;
    entry.mouseWasInside = inside;

    IntRect r = ToIntRect(panel->bounds);

    PanelCachePolicy::Input input;
    input.enabled = true;
    input.refreshInterval = interval;
    input.surfaceValid = entry.surface.valid;
    input.geometryChanged = entry.surface.x != r.x || entry.surface.y != r.y
        || entry.surface.width != r.width || entry.surface.height != r.height;
    input.framesSinceShown = frameIndex - entry.lastShownFrame;
    input.dirtyAllFrames = dirtyAllFrames;
    input.mouseInside = inside;
    input.mouseWasInside = wasInside;
    input.focused = WidgetInput::GetFocusOwner(WidgetInput::kEditorDevice) == static_cast<const void*>(panel);
    input.secondsSinceRender = GetTime() - entry.lastRenderTime;

    if (!PanelCachePolicy::CanReuse(input)) return false;

    DrawSurface(entry.surface);
    entry.lastShownFrame = frameIndex;
    return true;
}

void Capture(Panel* panel) {
    if (panel->CacheRefreshInterval() < 0.0f || DisabledByEnvironment()) return;

    Entry& entry = entries[panel];
    entry.lastShownFrame = frameIndex;
    entry.lastRenderTime = GetTime();
    if (!CaptureScreenRect(entry.surface, panel->bounds)) entry.surface.valid = false;
}

}
