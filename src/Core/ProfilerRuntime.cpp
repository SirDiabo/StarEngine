#include "ProfilerRuntime.h"
#include "ProfilerCapture.h"
#include "ProfilerGpu.h"
#include "SavePaths.h"
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace ProfilerRuntime {

namespace {

bool g_enabled = false;
bool g_overlayVisible = false;
bool g_hotkeys = true;
bool g_captureAllowed = true;
std::string g_appName;

std::string Lower(std::string text) {
    for (char& c : text) c = (char)std::tolower((unsigned char)c);
    return text;
}

bool StartsWith(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool ParseDetail(const std::string& value, int& out) {
    const std::string v = Lower(value);
    if (v == "standard" || v == "0") { out = Profiler::DetailStandard; return true; }
    if (v == "nodes" || v == "1") { out = Profiler::DetailNodes; return true; }
    if (v == "verbose" || v == "2") { out = Profiler::DetailVerbose; return true; }
    return false;
}

std::string RendererName() {
    switch (rlGetVersion()) {
        case RL_OPENGL_SOFTWARE: return "Software (rlsw CPU rasterizer)";
        case RL_OPENGL_11: return "OpenGL 1.1";
        case RL_OPENGL_21: return "OpenGL 2.1";
        case RL_OPENGL_33: return "OpenGL 3.3";
        case RL_OPENGL_43: return "OpenGL 4.3";
        case RL_OPENGL_ES_20: return "OpenGL ES 2.0";
        case RL_OPENGL_ES_30: return "OpenGL ES 3.0";
        default: return "Unknown";
    }
}

std::string SanitizeName(const std::string& name) {
    std::string result;
    for (char c : name) result.push_back((std::isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_');
    return result.empty() ? std::string("game") : result;
}

bool StartCapture(const std::string& path, const std::string& appName) {
    ProfilerCapture::Options options;
    options.path = path;
    options.appName = appName;
    options.renderer = RendererName();
    options.gpuTiming = ProfilerGpu::Available();
    const bool started = ProfilerCapture::Start(options);
    if (started) TraceLog(LOG_INFO, "PROFILER: capturing to %s", path.c_str());
    else TraceLog(LOG_ERROR, "PROFILER: cannot start capture: %s", ProfilerCapture::LastError().c_str());
    return started;
}

int OverlayScale() {
    int scale = GetScreenHeight() / 700;
    return scale < 1 ? 1 : (scale > 3 ? 3 : scale);
}

float NiceMax(float value) {
    if (value <= 1.0f) return 1.0f;
    const float base = std::pow(10.0f, std::floor(std::log10(value)));
    const float mantissa = value / base;
    float nice = 10.0f;
    if (mantissa <= 1.0f) nice = 1.0f;
    else if (mantissa <= 2.0f) nice = 2.0f;
    else if (mantissa <= 5.0f) nice = 5.0f;
    return nice * base;
}

}

Options ParseArguments(int argc, const char* const* argv, const char* environmentValue) {
    Options options;

    if (environmentValue && *environmentValue) {
        const std::string env = Lower(environmentValue);
        if (env == "0" || env == "false" || env == "off") {
            options.enabled = false;
        } else {
            options.enabled = true;
            if (env != "1" && env != "true" && env != "on") options.outputPath = environmentValue;
        }
    }

    for (int i = 1; i < argc; i++) {
        if (!argv[i]) continue;
        std::string arg = argv[i];
        if (!StartsWith(arg, "-")) continue;
        while (StartsWith(arg, "-")) arg.erase(0, 1);
        const std::string lowered = Lower(arg);
        if (!StartsWith(lowered, "profile")) continue;

        std::string value;
        std::string key = lowered;
        const size_t equals = arg.find('=');
        if (equals != std::string::npos) {
            key = Lower(arg.substr(0, equals));
            value = arg.substr(equals + 1);
        }

        if (key == "profile") {
            options.enabled = true;
        } else if (key == "profile-out") {
            if (equals == std::string::npos && i + 1 < argc && argv[i + 1]) value = argv[++i];
            if (value.empty()) options.warnings.push_back("-profile-out needs a path");
            else { options.outputPath = value; options.enabled = true; }
        } else if (key == "profile-detail") {
            if (equals == std::string::npos && i + 1 < argc && argv[i + 1]) value = argv[++i];
            int level = 0;
            if (ParseDetail(value, level)) options.detail = level;
            else options.warnings.push_back("-profile-detail expects standard, nodes or verbose (got '" + value + "')");
        } else if (key == "profile-overlay") {
            options.overlay = true;
            options.enabled = true;
        } else if (key == "profile-no-capture") {
            options.capture = false;
            options.enabled = true;
        } else if (key == "profile-no-hotkeys") {
            options.hotkeys = false;
        } else {
            options.warnings.push_back("unknown option '-" + arg + "'");
        }
    }
    return options;
}

std::string DefaultCapturePath(const std::string& appName) {
    std::filesystem::path directory = std::filesystem::path(SavePaths::GetSaveDirectory()).parent_path() / "Profiles";
    return (directory / ProfilerCapture::TimestampedName(SanitizeName(appName))).string();
}

std::vector<Hotspot> ComputeHotspots(const Profiler::Frame& frame, size_t limit) {
    std::vector<Hotspot> spots;
    for (const Profiler::Node& node : frame.nodes) {
        if (node.flags & Profiler::FlagWait) continue;
        if (node.selfNs <= 0) continue;
        Hotspot spot;
        spot.name = Profiler::NameOf(node.nameId);
        spot.selfNs = node.selfNs;
        spot.totalNs = node.totalNs;
        spot.lane = node.lane;
        spots.push_back(std::move(spot));
    }
    std::stable_sort(spots.begin(), spots.end(), [](const Hotspot& a, const Hotspot& b) { return a.selfNs > b.selfNs; });
    if (spots.size() > limit) spots.resize(limit);
    return spots;
}

bool Initialize(const Options& options, const std::string& appName) {
    for (const std::string& warning : options.warnings) TraceLog(LOG_WARNING, "PROFILER: %s", warning.c_str());
    if (!options.enabled) return false;

    g_enabled = true;
    g_overlayVisible = options.overlay;
    g_hotkeys = options.hotkeys;
    g_captureAllowed = options.capture;
    g_appName = appName;

    Profiler::SetForcedRecording(true);
    Profiler::SetDetail(options.detail);
    TraceLog(LOG_INFO, "PROFILER: enabled (detail=%d, renderer=%s, gpu timing=%s)", options.detail,
             RendererName().c_str(), ProfilerGpu::Available() ? "on" : "off");

    if (options.capture) {
        const std::string path = options.outputPath.empty() ? DefaultCapturePath(appName) : options.outputPath;
        StartCapture(path, appName);
    }
    return true;
}

void BeginFrame() {
    Profiler::BeginFrame();
    if (!g_enabled) return;
    ProfilerGpu::Pump();
    ProfilerCapture::Pump();

    if (g_hotkeys) {
        if (IsKeyPressed(KEY_F10)) g_overlayVisible = !g_overlayVisible;
        if (IsKeyPressed(KEY_F9) && g_captureAllowed) {
            if (ProfilerCapture::IsActive()) {
                ProfilerCapture::Stop();
                TraceLog(LOG_INFO, "PROFILER: capture stopped (%llu bytes)", (unsigned long long)ProfilerCapture::BytesWritten());
            } else {
                StartCapture(DefaultCapturePath(g_appName), g_appName);
            }
        }
    }
}

void PreEndDrawing() {
    if (!g_enabled || !Profiler::IsEnabled()) return;
    {
        PROFILE_SCOPE("Flush Render Batch");
        rlDrawRenderBatchActive();
    }
    ProfilerGpu::FrameEnd();
}

void DrawOverlay() {
    if (!g_enabled || !g_overlayVisible) return;
    PROFILE_SCOPE("Profiler Overlay");

    const int s = OverlayScale();
    const int fontSize = 10 * s;
    const int lineHeight = 13 * s;
    const int hotspotRows = 8;
    const int x0 = 10 * s;
    const int y0 = 10 * s;
    const int width = 470 * s;
    const int graphHeight = 70 * s;
    const int height = (6 + 13 + 13 + 6) * s + graphHeight + (8 + 13 + hotspotRows * 13 + 6 + 13 + 6) * s;

    DrawRectangle(x0, y0, width, height, { 0, 0, 0, 190 });
    DrawRectangleLines(x0, y0, width, height, { 90, 90, 100, 255 });

    const Color textColor = { 230, 230, 235, 255 };
    const Color dimColor = { 150, 150, 160, 255 };
    int y = y0 + 6 * s;
    const int x = x0 + 8 * s;

    DrawText("PROFILER", x, y, fontSize, { 120, 255, 200, 255 });
    DrawText("F10 hide   F9 capture on/off", x + 80 * s, y, fontSize, dimColor);
    y += lineHeight;

    const Profiler::Frame* frame = Profiler::FrameAt(Profiler::DisplayAge());
    if (!frame) {
        DrawText("collecting frames...", x, y, fontSize, dimColor);
        return;
    }

    char line[200];
    const double wall = (double)frame->wallNs / 1e9;
    snprintf(line, sizeof(line), "%s (%.0f fps)  work %s  wait %s%s%s",
             Profiler::FormatDuration(frame->wallNs).c_str(), wall > 0.0 ? 1.0 / wall : 0.0,
             Profiler::FormatDuration(frame->workNs).c_str(), Profiler::FormatDuration(frame->waitNs).c_str(),
             frame->gpuResolved ? "  gpu " : "", frame->gpuResolved ? Profiler::FormatDuration(frame->gpuSpanNs).c_str() : "");
    DrawText(line, x, y, fontSize, textColor);
    y += lineHeight + 4 * s;

    const Rectangle plot = { (float)x, (float)y, (float)(width - 16 * s), (float)graphHeight };
    DrawRectangleRec(plot, { 20, 20, 24, 255 });
    const size_t window = std::min<size_t>(240, Profiler::FrameCount());
    static std::vector<Vector2> points;
    if (window >= 2) {
        float maxUs = 2000.0f;
        for (size_t age = 0; age < window; age++) {
            const Profiler::Frame* f = Profiler::FrameAt(age);
            maxUs = std::max(maxUs, (float)((double)f->workNs / 1000.0));
            if (f->gpuResolved) maxUs = std::max(maxUs, (float)((double)f->gpuSpanNs / 1000.0));
        }
        const float yMax = NiceMax(maxUs * 1.05f);
        const float budget = 1000000.0f / 60.0f;
        if (budget <= yMax) {
            const float by = plot.y + plot.height - (budget / yMax) * plot.height;
            DrawLineEx({ plot.x, by }, { plot.x + plot.width, by }, 1.0f, { 200, 70, 70, 200 });
        }
        const float step = plot.width / (float)(window - 1);
        for (int pass = 0; pass < 2; pass++) {
            points.clear();
            for (size_t i = 0; i < window; i++) {
                const Profiler::Frame* f = Profiler::FrameAt(window - 1 - i);
                float value;
                if (pass == 0) {
                    value = (float)((double)f->workNs / 1000.0);
                } else {
                    if (!f->gpuResolved) continue;
                    value = (float)((double)f->gpuSpanNs / 1000.0);
                }
                points.push_back({ plot.x + step * (float)i, plot.y + plot.height - std::min(1.0f, value / yMax) * plot.height });
            }
            if (points.size() >= 2) DrawLineStrip(points.data(), (int)points.size(), pass == 0 ? Color{ 255, 255, 255, 255 } : Color{ 120, 255, 200, 255 });
        }
        DrawText(Profiler::FormatDuration((int64_t)(yMax * 1000.0f)).c_str(), (int)plot.x + 3 * s, (int)plot.y + 2 * s, fontSize, dimColor);
    }
    y += graphHeight + 8 * s;

    DrawText("self time hotspots (this frame)", x, y, fontSize, dimColor);
    y += lineHeight;
    const std::vector<Hotspot> spots = ComputeHotspots(*frame, (size_t)hotspotRows);
    for (const Hotspot& spot : spots) {
        const std::string time = Profiler::FormatDuration(spot.selfNs);
        DrawText(time.c_str(), x, y, fontSize, textColor);
        std::string label = spot.name;
        if (spot.lane == Profiler::Lane::GPU) label += " (GPU)";
        if (label.size() > 52) label = label.substr(0, 49) + "...";
        DrawText(label.c_str(), x + 62 * s, y, fontSize, spot.lane == Profiler::Lane::GPU ? Color{ 120, 255, 200, 255 } : textColor);
        y += lineHeight;
    }
    y += 6 * s;

    if (ProfilerCapture::IsActive()) {
        snprintf(line, sizeof(line), "REC %s (%.1f MB)", std::filesystem::path(ProfilerCapture::ActivePath()).filename().string().c_str(),
                 (double)ProfilerCapture::BytesWritten() / (1024.0 * 1024.0));
        DrawText(line, x, y, fontSize, { 255, 110, 110, 255 });
    } else {
        DrawText(ProfilerCapture::LastError().empty() ? "capture off" : ProfilerCapture::LastError().c_str(), x, y, fontSize, dimColor);
    }
}

void Shutdown() {
    if (!g_enabled) return;
    if (ProfilerCapture::IsActive()) {
        ProfilerCapture::Stop();
        TraceLog(LOG_INFO, "PROFILER: capture saved (%llu bytes)", (unsigned long long)ProfilerCapture::BytesWritten());
    }
    g_enabled = false;
}

bool IsEnabled() { return g_enabled; }
bool IsOverlayVisible() { return g_overlayVisible; }

}
