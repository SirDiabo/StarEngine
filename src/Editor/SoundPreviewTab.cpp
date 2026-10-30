#include "SoundPreviewTab.h"
#include "ImportDetailsPanel.h"
#include "MenuBar.h"
#include "UITheme.h"
#include "Core/StarAsset.h"
#include "Core/CookedFile.h"
#include "Core/Widgets/WidgetInput.h"
#include "Runtime/Audio/WavFile.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

constexpr float kRightPanelWidth = 300.0f;
constexpr float kTopBarHeight = 28.0f;
constexpr int kPeakBuckets = 1024;
constexpr uint64_t kPeakWindow = 512;

bool Button(Rectangle r, const char* label, bool active) {
    const UIPalette& theme = UITheme::Get();
    bool hovered = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, active ? theme.accent : (hovered ? theme.buttonHover : theme.buttonBg));
    DrawRectangleLinesEx(r, 1, theme.buttonBorder);
    int tw = MeasureText(label, 11);
    DrawText(label, (int)(r.x + (r.width - tw) / 2.0f), (int)(r.y + (r.height - 11) / 2.0f), 11, theme.textPrimary);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

}

SoundPreviewTab::SoundPreviewTab(const std::string& path) : assetPath(path) {
    title = fs::path(path).stem().string();
    permanent = false;
}

SoundPreviewTab::~SoundPreviewTab() {
    StopPlayback();
    ImportDetailsPanel::Get().Close();
}

void SoundPreviewTab::Initialize() {
    BuildWaveform();
    ImportDetailsPanel::Get().Open(assetPath, "Sound", true);
}

void SoundPreviewTab::BuildWaveform() {
    peakMin.assign(kPeakBuckets, 0.0f);
    peakMax.assign(kPeakBuckets, 0.0f);
    formatText.clear();
    duration = 0.0;

    StarAsset asset;
    if (!StarAsset::LoadFromFile(assetPath, asset)) return;
    std::string resolved = CookedFile::Open(asset.ResolveSourcePath());
    WavInfo info;
    std::string error;
    if (resolved.empty() || !WavFile::ReadInfo(resolved, info, error)) {
        formatText = "Could not read sound: " + error;
        return;
    }

    duration = info.DurationSeconds();
    char buf[96];
    snprintf(buf, sizeof(buf), "%d Hz, %s, %d-bit", info.sampleRate, info.channels >= 2 ? "stereo" : "mono", info.bitsPerSample);
    formatText = buf;

    std::ifstream file(resolved, std::ios::binary);
    std::vector<float> window((size_t)kPeakWindow);
    for (int b = 0; b < kPeakBuckets; b++) {
        uint64_t start = info.frameCount * (uint64_t)b / kPeakBuckets;
        uint64_t end = info.frameCount * (uint64_t)(b + 1) / kPeakBuckets;
        uint64_t count = std::min<uint64_t>(end > start ? end - start : 1, kPeakWindow);
        uint64_t got = WavFile::ReadFrames(file, info, start, count, window.data(), 1);
        float lo = 0.0f, hi = 0.0f;
        for (uint64_t i = 0; i < got; i++) {
            lo = std::min(lo, window[(size_t)i]);
            hi = std::max(hi, window[(size_t)i]);
        }
        peakMin[(size_t)b] = lo;
        peakMax[(size_t)b] = hi;
    }
}

bool SoundPreviewTab::IsPlaying() const {
    return source && source->IsPlaying();
}

void SoundPreviewTab::StartPlayback() {
    source = AudioSourceRegistry::Get().Create(assetPath);
    if (!source) return;
    source->SetLooping(looping);
    source->Play();
}

void SoundPreviewTab::StopPlayback() {
    if (source) source->Stop();
}

void SoundPreviewTab::OnDeactivated() {
    StopPlayback();
}

void SoundPreviewTab::Render(float x, float y, float width, float height) {
    const UIPalette& theme = UITheme::Get();

    Rectangle top = {x, y, width, kTopBarHeight};
    DrawRectangleRec(top, {36, 36, 40, 255});
    DrawLine((int)top.x, (int)(top.y + top.height), (int)(top.x + top.width), (int)(top.y + top.height), {60, 60, 65, 255});
    DrawText(title.c_str(), (int)x + 6, (int)y + 8, 13, WHITE);

    float contentY = y + kTopBarHeight;
    float contentH = height - kTopBarHeight;
    float viewW = std::max(100.0f, width - kRightPanelWidth);

    Rectangle transport = {x + 8, contentY + 8, viewW - 16, 24};
    bool playing = IsPlaying();
    if (Button({transport.x, transport.y, 70, transport.height}, playing ? "Stop" : "Play", playing)) {
        if (playing) StopPlayback(); else StartPlayback();
    }

    Rectangle loopBox = {transport.x + 82, transport.y + 4, 16, 16};
    bool loopHover = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), {loopBox.x, loopBox.y, 70, 16});
    DrawRectangleRec(loopBox, theme.fieldBg);
    DrawRectangleLinesEx(loopBox, 1, theme.fieldBorder);
    if (looping) DrawRectangle((int)loopBox.x + 4, (int)loopBox.y + 4, 8, 8, theme.accent);
    DrawText("Loop", (int)loopBox.x + 22, (int)loopBox.y + 2, 11, theme.textPrimary);
    if (loopHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        looping = !looping;
        if (source) source->SetLooping(looping);
    }

    double position = source ? source->GetPosition() : 0.0;
    char timeText[64];
    snprintf(timeText, sizeof(timeText), "%.2f / %.2f s", position, duration);
    DrawText(timeText, (int)(transport.x + 150), (int)transport.y + 6, 11, theme.textPrimary);
    DrawText(formatText.c_str(), (int)(transport.x + 270), (int)transport.y + 6, 11, theme.textSecondary);

    Rectangle wave = {x + 8, contentY + 44, viewW - 16, std::max(40.0f, contentH - 52.0f)};
    DrawRectangleRec(wave, theme.fieldBg);
    DrawRectangleLinesEx(wave, 1, theme.fieldBorder);

    float mid = wave.y + wave.height * 0.5f;
    float amp = wave.height * 0.5f - 4.0f;
    DrawLine((int)wave.x, (int)mid, (int)(wave.x + wave.width), (int)mid, {70, 70, 75, 255});
    int columns = (int)wave.width - 2;
    for (int c = 0; c < columns; c++) {
        int bucket = std::min(kPeakBuckets - 1, (int)((long long)c * kPeakBuckets / std::max(1, columns)));
        float lo = peakMin[(size_t)bucket], hi = peakMax[(size_t)bucket];
        float px = wave.x + 1 + (float)c;
        DrawLine((int)px, (int)(mid - hi * amp), (int)px, (int)(mid - lo * amp) + 1, theme.accent);
    }

    if (duration > 0.0) {
        bool overWave = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), wave);
        if (overWave && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) draggingPlayhead = true;
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) draggingPlayhead = false;
        if (draggingPlayhead) {
            float t = std::clamp((GetMousePosition().x - wave.x) / wave.width, 0.0f, 1.0f);
            if (!source || !source->IsPlaying()) StartPlayback();
            if (source) source->Seek((double)t * duration);
        }

        double shown = source ? source->GetPosition() : 0.0;
        float px = wave.x + (float)(shown / duration) * wave.width;
        DrawLine((int)px, (int)wave.y, (int)px, (int)(wave.y + wave.height), WHITE);
    }

    Rectangle rightPanel = {x + viewW, contentY, kRightPanelWidth, contentH};
    ImportDetailsPanel::Get().RenderInline(rightPanel);
}

void SoundPreviewTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { "Save", "Save All" } },
        { "Asset",  { "Find in Content Browser" } },
        { "Window", { } },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}
