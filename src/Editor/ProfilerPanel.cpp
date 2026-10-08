#include "ProfilerPanel.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

const Color kPalette[12] = {
    { 235, 90, 90, 255 },  { 90, 205, 125, 255 }, { 95, 155, 255, 255 }, { 240, 195, 65, 255 },
    { 195, 105, 240, 255 }, { 75, 215, 215, 255 }, { 240, 135, 65, 255 }, { 165, 225, 85, 255 },
    { 240, 115, 185, 255 }, { 135, 135, 255, 255 }, { 205, 205, 125, 255 }, { 125, 185, 165, 255 },
};

const float kToolbarHeight = 22.0f;
const float kStatsHeight = 14.0f;
const float kRowHeight = 16.0f;
const float kHeaderHeight = 16.0f;
const float kPad = 6.0f;

bool Hovered(Rectangle r) {
    return !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), r);
}

bool Button(Rectangle r, const char* label, bool active) {
    const UIPalette& theme = UITheme::Get();
    bool hovered = Hovered(r);
    Color bg = active ? theme.surfaceSelected : (hovered ? theme.buttonHover : theme.buttonBg);
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 1, active ? theme.accent : theme.buttonBorder);
    int tw = MeasureText(label, 10);
    DrawText(label, (int)(r.x + (r.width - tw) / 2.0f), (int)(r.y + (r.height - 10) / 2.0f), 10, theme.textPrimary);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

float ButtonWidth(const char* label) {
    return (float)MeasureText(label, 10) + 16.0f;
}

void DrawRightAligned(const std::string& text, float rightEdge, float y, Color color) {
    int tw = MeasureText(text.c_str(), 10);
    DrawText(text.c_str(), (int)(rightEdge - tw), (int)y, 10, color);
}

std::string ClipText(const std::string& text, float maxWidth) {
    if ((float)MeasureText(text.c_str(), 10) <= maxWidth) return text;
    std::string clipped = text;
    while (!clipped.empty() && (float)MeasureText((clipped + "...").c_str(), 10) > maxWidth) clipped.pop_back();
    return clipped + "...";
}

std::string Percent(double value) {
    if (value < 0.0) return "-";
    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f%%", value);
    return buf;
}

std::string Duration(int64_t ns) { return Profiler::FormatDuration(ns); }
std::string DurationUs(float us) { return Profiler::FormatDuration((int64_t)((double)us * 1000.0)); }

}

ProfilerPanel::~ProfilerPanel() {
    delete chrome;
}

void ProfilerPanel::Initialize() {
    name = "Profiler";
}

void ProfilerPanel::EnsureWidgets() {
    if (chrome) return;
    const UIPalette& theme = UITheme::Get();
    chrome = new Widget();
    auto* root = new WidgetElement();
    chrome->SetRoot(root);
    lineCountField = new TextElement();
    lineCountField->editable = true;
    lineCountField->numericOnly = true;
    lineCountField->fontSize = 12.0f;
    lineCountField->maxLength = 3;
    lineCountField->backgroundColor = theme.fieldBg;
    lineCountField->borderColor = theme.fieldBorder;
    lineCountField->editingBorderColor = theme.accent;
    lineCountField->onTextCommitted = [this](const std::string& text) {
        int value = atoi(text.c_str());
        lineCount = std::max(0, std::min(64, value));
    };
    root->AddChild(lineCountField);
}

const Profiler::Frame* ProfilerPanel::DisplayFrame() const {
    if (frozen) return &frozenFrame;
    return Profiler::FrameAt(0);
}

void ProfilerPanel::FreezeFrameAtAge(size_t age) {
    const Profiler::Frame* frame = Profiler::FrameAt(age);
    if (!frame) return;
    frozenFrame = *frame;
    frozen = true;
}

void ProfilerPanel::ReturnToLive() {
    frozen = false;
}

Color ProfilerPanel::LineColor(uint64_t key) {
    if (key == ProfilerModel::kKeyWork) return UITheme::Get().textPrimary;
    return kPalette[colors.Get(key) % 12];
}

void ProfilerPanel::Render() {
    EnsureWidgets();
    if (recording) Profiler::RequestRecording();
    Profiler::SetDetail(detailLevel);

    if (!initializedExpansion) {
        expanded.insert(ProfilerModel::kKeyFrame);
        expanded.insert(ProfilerModel::kKeyWork);
        initializedExpansion = true;
    }

    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(bounds, theme.panelBg);

    Rectangle inner = { bounds.x + kPad, bounds.y + kPad, bounds.width - kPad * 2.0f, bounds.height - kPad * 2.0f };
    if (inner.width < 200.0f || inner.height < 120.0f) {
        DrawText("Panel too small", (int)inner.x, (int)inner.y, 10, theme.textSecondary);
        return;
    }

    {
        PROFILE_SCOPE("Toolbar");
        DrawToolbar({ inner.x, inner.y, inner.width, kToolbarHeight });
    }

    const Profiler::Frame* frame = DisplayFrame();
    DrawStats({ inner.x, inner.y + kToolbarHeight + 3.0f, inner.width, kStatsHeight }, frame);

    float bodyY = inner.y + kToolbarHeight + kStatsHeight + 8.0f;
    float bodyH = inner.y + inner.height - bodyY;

    if (!frame) {
        DrawText(recording ? "Waiting for frames..." : "Recording is paused. Press Record to capture frames.",
                  (int)inner.x, (int)bodyY + 6, 11, theme.textSecondary);
        return;
    }

    std::vector<ProfilerModel::Row> rows;
    ProfilerModel::BuildOptions options;
    options.laneFilter = laneFilter;
    options.expanded = &expanded;
    {
        PROFILE_SCOPE("Build Rows");
        ProfilerModel::BuildRows(*frame, options, rows);
    }

    float graphH = std::max(90.0f, bodyH * 0.40f);
    if (graphH > bodyH - 60.0f) graphH = std::max(40.0f, bodyH - 60.0f);
    Rectangle graphArea = { inner.x, bodyY, inner.width, graphH };
    Rectangle tableArea = { inner.x, bodyY + graphH + 6.0f, inner.width, bodyH - graphH - 6.0f };

    Tooltip tooltip;
    {
        PROFILE_SCOPE("Graph");
        DrawGraph(graphArea, rows, tooltip);
    }
    {
        PROFILE_SCOPE("Table");
        DrawTable(tableArea, rows);
    }
    {
        PROFILE_SCOPE("Tooltip");
        DrawTooltip(tooltip);
    }
}

void ProfilerPanel::DrawToolbar(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    float x = area.x;
    float y = area.y;
    float h = area.height;

    const char* recordLabel = recording ? "Recording" : "Paused";
    float w = ButtonWidth(recordLabel) + 10.0f;
    if (Button({ x, y, w, h }, recordLabel, recording)) recording = !recording;
    DrawCircle((int)(x + 9), (int)(y + h / 2), 3.0f, recording ? theme.danger : theme.textSecondary);
    x += w + 4.0f;

    w = ButtonWidth("Clear");
    if (Button({ x, y, w, h }, "Clear", false)) {
        Profiler::Clear();
        frozen = false;
        colors.Reset();
        tableScroll = 0.0f;
    }
    x += w + 4.0f;

    if (frozen) {
        w = ButtonWidth("Go Live");
        if (Button({ x, y, w, h }, "Go Live", false)) ReturnToLive();
        x += w + 4.0f;
    }

    x += 6.0f;
    DrawText("Lines:", (int)x, (int)(y + (h - 10) / 2.0f), 10, theme.textSecondary);
    x += (float)MeasureText("Lines:", 10) + 4.0f;
    Rectangle fieldRect = { x, y, 34.0f, h };
    if (!lineCountField->isEditing) lineCountField->text = std::to_string(lineCount);
    lineCountField->size = { fieldRect.width, fieldRect.height };
    lineCountField->offset = { fieldRect.x, fieldRect.y };
    float dt = GetFrameTime();
    Rectangle chromeRect = { 0, 0, bounds.x + bounds.width, bounds.y + bounds.height };
    chrome->Tick(dt, chromeRect, 1.0f);
    WidgetInput::ProcessInput(chrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    chrome->Draw(chromeRect, 1.0f);
    x += fieldRect.width + 10.0f;

    const char* detailLabel = detailLevel == Profiler::DetailNodes ? "Detail: Nodes" : "Detail: Standard";
    w = ButtonWidth("Detail: Standard");
    if (Button({ x, y, w, h }, detailLabel, detailLevel != Profiler::DetailStandard)) {
        detailLevel = detailLevel == Profiler::DetailStandard ? Profiler::DetailNodes : Profiler::DetailStandard;
    }
    x += w + 10.0f;

    struct LaneButton { const char* label; ProfilerModel::LaneFilter value; };
    const LaneButton laneButtons[3] = {
        { "All", ProfilerModel::LaneFilter::All },
        { "CPU", ProfilerModel::LaneFilter::CPU },
        { "GPU", ProfilerModel::LaneFilter::GPU },
    };
    for (const LaneButton& lb : laneButtons) {
        w = ButtonWidth(lb.label);
        if (Button({ x, y, w, h }, lb.label, laneFilter == lb.value)) laneFilter = lb.value;
        x += w + 2.0f;
    }
    x += 8.0f;

    w = ButtonWidth("Expand Hottest");
    if (x + w < area.x + area.width) {
        if (Button({ x, y, w, h }, "Expand Hottest", false)) {
            const Profiler::Frame* frame = DisplayFrame();
            if (frame) ProfilerModel::ExpandHottestPath(*frame, expanded);
        }
        x += w + 4.0f;
    }
    w = ButtonWidth("Collapse All");
    if (x + w < area.x + area.width) {
        if (Button({ x, y, w, h }, "Collapse All", false)) {
            expanded.clear();
            expanded.insert(ProfilerModel::kKeyFrame);
            expanded.insert(ProfilerModel::kKeyWork);
            tableScroll = 0.0f;
        }
    }
}

void ProfilerPanel::DrawStats(Rectangle area, const Profiler::Frame* frame) {
    const UIPalette& theme = UITheme::Get();
    std::string text;
    Color color = theme.textSecondary;
    if (frame) {
        char buf[64];
        double wallSeconds = (double)frame->wallNs / 1e9;
        snprintf(buf, sizeof(buf), "%.0f fps", wallSeconds > 0.0 ? 1.0 / wallSeconds : 0.0);
        double overheadNs = (double)frame->totalCalls * Profiler::OverheadNsPerScope();
        text = std::string(frozen ? "FROZEN  " : "") + "Frame #" + std::to_string(frame->index) +
               "   Wall " + Duration(frame->wallNs) + " (" + buf + ")" +
               "   Work " + Duration(frame->workNs) +
               "   Wait " + Duration(frame->waitNs) +
               "   Scopes " + std::to_string(frame->totalCalls) +
               " (~" + Duration((int64_t)overheadNs) + " profiler overhead)";
        if (frozen) color = theme.accent;
    }
    const bool software = rlGetVersion() == RL_OPENGL_SOFTWARE;
    std::string renderer = software ? "Renderer: Software (CPU rasterizer)" : "Renderer: OpenGL";
    DrawText(text.c_str(), (int)area.x, (int)area.y + 2, 10, color);
    DrawRightAligned(renderer, area.x + area.width, area.y + 2, theme.textSecondary);
}

void ProfilerPanel::DrawGraph(Rectangle area, const std::vector<ProfilerModel::Row>& rows, Tooltip& tooltip) {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(area, ColorBrightness(theme.panelBg, -0.12f));
    DrawRectangleLinesEx(area, 1, theme.panelBorder);

    const size_t window = std::min(Profiler::kHistoryFrames, Profiler::FrameCount());
    Rectangle plot = { area.x + 58.0f, area.y + 18.0f, area.width - 58.0f - 10.0f, area.height - 18.0f - 16.0f };
    if (window < 2 || plot.width < 40.0f || plot.height < 20.0f) {
        DrawText("Collecting samples...", (int)area.x + 8, (int)area.y + 8, 10, theme.textSecondary);
        return;
    }

    std::vector<uint64_t> lineKeys;
    lineKeys.push_back(ProfilerModel::kKeyWork);
    for (uint64_t key : ProfilerModel::SelectLineKeys(rows, (size_t)std::max(0, lineCount), pinned, hidden)) {
        lineKeys.push_back(key);
    }

    drawnLines.clear();
    std::vector<std::vector<float>> series(lineKeys.size());
    float dataMax = 0.0f;
    for (size_t i = 0; i < lineKeys.size(); i++) {
        ProfilerModel::FillSeries(lineKeys[i], window, series[i]);
        if (lineKeys[i] != ProfilerModel::kKeyWork) drawnLines.insert(lineKeys[i]);
        for (float v : series[i]) dataMax = std::max(dataMax, v);
    }

    float rangeMax = dataMax * 1.08f;
    if (dataMax > ProfilerModel::kBudgetUs60Fps * 0.5f) rangeMax = std::max(rangeMax, ProfilerModel::kBudgetUs60Fps * 1.05f);
    const float yMax = ProfilerModel::NiceMaxUs(rangeMax);

    const int divisions = 4;
    for (int d = 0; d <= divisions; d++) {
        float t = (float)d / (float)divisions;
        float y = plot.y + plot.height - t * plot.height;
        DrawLineEx({ plot.x, y }, { plot.x + plot.width, y }, 1.0f, ColorAlpha(theme.panelBorder, d == 0 ? 0.9f : 0.45f));
        std::string label = DurationUs(yMax * t);
        DrawRightAligned(label, plot.x - 4.0f, y - 5.0f, theme.textSecondary);
    }

    if (ProfilerModel::kBudgetUs60Fps <= yMax) {
        float y = plot.y + plot.height - (ProfilerModel::kBudgetUs60Fps / yMax) * plot.height;
        for (float x = plot.x; x < plot.x + plot.width; x += 8.0f) {
            DrawLineEx({ x, y }, { std::min(x + 4.0f, plot.x + plot.width), y }, 1.0f, ColorAlpha(theme.danger, 0.8f));
        }
        DrawText("16.67ms (60 fps)", (int)(plot.x + plot.width - MeasureText("16.67ms (60 fps)", 10) - 2), (int)y - 11, 10, ColorAlpha(theme.danger, 0.9f));
    }

    const float step = plot.width / (float)(window - 1);
    linePoints.resize(window);
    for (size_t li = 0; li < lineKeys.size(); li++) {
        Color color = LineColor(lineKeys[li]);
        for (size_t i = 0; i < window; i++) {
            float x = plot.x + step * (float)i;
            float y = plot.y + plot.height - std::min(1.0f, series[li][i] / yMax) * plot.height;
            linePoints[i] = { x, y };
        }
        DrawLineStrip(linePoints.data(), (int)window, color);
        if (lineKeys[li] == ProfilerModel::kKeyWork) {
            for (Vector2& point : linePoints) point.y += 1.0f;
            DrawLineStrip(linePoints.data(), (int)window, color);
        }
    }

    if (frozen) {
        for (size_t age = 0; age < window; age++) {
            const Profiler::Frame* f = Profiler::FrameAt(age);
            if (f && f->index == frozenFrame.index) {
                float x = plot.x + step * (float)(window - 1 - age);
                DrawLineEx({ x, plot.y }, { x, plot.y + plot.height }, 1.0f, theme.accent);
                break;
            }
        }
    }

    float legendX = plot.x;
    for (size_t li = 0; li < lineKeys.size(); li++) {
        std::string name;
        if (lineKeys[li] == ProfilerModel::kKeyWork) name = "Work";
        else if (const Profiler::Track* track = Profiler::FindTrack(lineKeys[li])) name = Profiler::NameOf(track->nameId);
        else continue;
        name = ClipText(name, 130.0f);
        float itemW = (float)MeasureText(name.c_str(), 10) + 18.0f;
        if (legendX + itemW > plot.x + plot.width) break;
        DrawRectangle((int)legendX, (int)area.y + 6, 8, 8, LineColor(lineKeys[li]));
        DrawText(name.c_str(), (int)legendX + 12, (int)area.y + 5, 10, theme.textPrimary);
        legendX += itemW;
    }

    DrawText(("-" + std::to_string(window) + " frames").c_str(), (int)plot.x, (int)(area.y + area.height - 12), 10, theme.textSecondary);
    DrawRightAligned("latest", plot.x + plot.width, area.y + area.height - 12, theme.textSecondary);

    Rectangle hitArea = { plot.x, plot.y, plot.width, plot.height };
    if (Hovered(hitArea)) {
        Vector2 mouse = GetMousePosition();
        int index = (int)std::lround((mouse.x - plot.x) / step);
        index = std::max(0, std::min((int)window - 1, index));
        size_t age = window - 1 - (size_t)index;
        const Profiler::Frame* hoverFrame = Profiler::FrameAt(age);
        float x = plot.x + step * (float)index;
        DrawLineEx({ x, plot.y }, { x, plot.y + plot.height }, 1.0f, ColorAlpha(theme.textPrimary, 0.5f));
        if (hoverFrame) {
            tooltip.active = true;
            tooltip.frameIndex = hoverFrame->index;
            for (size_t li = 0; li < lineKeys.size(); li++) {
                TooltipLine line;
                line.color = LineColor(lineKeys[li]);
                if (lineKeys[li] == ProfilerModel::kKeyWork) {
                    line.name = "Work";
                    line.value = Duration(hoverFrame->workNs);
                } else {
                    const Profiler::Track* track = Profiler::FindTrack(lineKeys[li]);
                    if (!track) continue;
                    float total = 0.0f, self = 0.0f;
                    track->Sample(hoverFrame->index, total, self);
                    line.name = Profiler::NameOf(track->nameId);
                    line.value = DurationUs(total);
                }
                tooltip.lines.push_back(line);
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) FreezeFrameAtAge(age);
        }
    }
}

void ProfilerPanel::DrawTooltip(const Tooltip& tooltip) {
    if (!tooltip.active) return;
    const UIPalette& theme = UITheme::Get();
    std::string header = "Frame #" + std::to_string(tooltip.frameIndex) + "  (click to freeze)";
    float width = (float)MeasureText(header.c_str(), 10);
    for (const TooltipLine& line : tooltip.lines) {
        float lw = (float)MeasureText(ClipText(line.name, 180.0f).c_str(), 10) + 70.0f + 14.0f;
        width = std::max(width, lw);
    }
    width += 16.0f;
    float height = 14.0f + (float)tooltip.lines.size() * 13.0f + 10.0f;
    Vector2 mouse = GetMousePosition();
    float x = mouse.x + 14.0f;
    float y = mouse.y + 14.0f;
    if (x + width > bounds.x + bounds.width) x = mouse.x - width - 8.0f;
    if (y + height > bounds.y + bounds.height) y = mouse.y - height - 8.0f;
    Rectangle box = { x, y, width, height };
    DrawRectangleRec(box, ColorAlpha(theme.windowBg, 0.96f));
    DrawRectangleLinesEx(box, 1, theme.panelBorder);
    DrawText(header.c_str(), (int)x + 8, (int)y + 5, 10, theme.textSecondary);
    float ly = y + 19.0f;
    for (const TooltipLine& line : tooltip.lines) {
        DrawRectangle((int)x + 8, (int)ly + 1, 8, 8, line.color);
        DrawText(ClipText(line.name, 180.0f).c_str(), (int)x + 22, (int)ly, 10, theme.textPrimary);
        DrawRightAligned(line.value, x + width - 8.0f, ly, theme.textPrimary);
        ly += 13.0f;
    }
}

void ProfilerPanel::DrawTable(Rectangle area, const std::vector<ProfilerModel::Row>& rows) {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(area, ColorBrightness(theme.panelBg, -0.06f));
    DrawRectangleLinesEx(area, 1, theme.panelBorder);

    if (laneFilter == ProfilerModel::LaneFilter::GPU) {
        const bool software = rlGetVersion() == RL_OPENGL_SOFTWARE;
        const char* line1 = software ? "Software renderer active: rasterization runs on the CPU."
                                     : "GPU timing is not recorded yet.";
        const char* line2 = software ? "See Work > Render Pipeline under the CPU / All view."
                                     : "Render passes are shown under Work > Render Pipeline (CPU submit time).";
        DrawText(line1, (int)area.x + 10, (int)area.y + 10, 11, theme.textPrimary);
        DrawText(line2, (int)area.x + 10, (int)area.y + 26, 10, theme.textSecondary);
        return;
    }

    const float scrollbarW = 8.0f;
    const float right = area.x + area.width - scrollbarW - 2.0f;
    const float colCalls = right - 46.0f;
    const float colMax = colCalls - 66.0f;
    const float colAvg = colMax - 66.0f;
    const float colPct = colAvg - 52.0f;
    const float colSelf = colPct - 66.0f;
    const float colTotal = colSelf - 66.0f;
    const float nameRight = colTotal - 4.0f;

    Rectangle header = { area.x, area.y, area.width, kHeaderHeight };
    DrawRectangleRec(header, ColorBrightness(theme.panelBg, -0.18f));
    DrawText("Name", (int)area.x + 8, (int)area.y + 3, 10, theme.textSecondary);
    DrawRightAligned("Total", colSelf - 4.0f, area.y + 3, theme.textSecondary);
    DrawRightAligned("Self", colPct - 4.0f, area.y + 3, theme.textSecondary);
    DrawRightAligned("% Work", colAvg - 4.0f, area.y + 3, theme.textSecondary);
    DrawRightAligned("Avg", colMax - 4.0f, area.y + 3, theme.textSecondary);
    DrawRightAligned("Max", colCalls - 4.0f, area.y + 3, theme.textSecondary);
    DrawRightAligned("Calls", right - 4.0f, area.y + 3, theme.textSecondary);

    Rectangle body = { area.x + 1, area.y + kHeaderHeight, area.width - 2, area.height - kHeaderHeight - 1 };
    const float contentH = (float)rows.size() * kRowHeight;
    const float maxScroll = std::max(0.0f, contentH - body.height);
    if (Hovered(body)) tableScroll -= GetMouseWheelMove() * kRowHeight * 3.0f;
    tableScroll = std::max(0.0f, std::min(maxScroll, tableScroll));

    BeginScissorMode((int)body.x, (int)body.y, (int)body.width, (int)body.height);
    int toggleRow = -1;
    bool swatchClicked = false;
    uint64_t swatchKey = 0;
    for (size_t i = 0; i < rows.size(); i++) {
        const ProfilerModel::Row& row = rows[i];
        float y = body.y + (float)i * kRowHeight - tableScroll;
        if (y + kRowHeight < body.y || y > body.y + body.height) continue;

        Rectangle rowRect = { body.x, y, body.width - scrollbarW, kRowHeight };
        bool rowHovered = Hovered(rowRect) && Hovered(body);
        if (row.key == selectedKey) DrawRectangleRec(rowRect, ColorAlpha(theme.surfaceSelected, 0.7f));
        else if (rowHovered) DrawRectangleRec(rowRect, ColorAlpha(theme.surfaceHover, 0.6f));
        else if (i % 2 == 1) DrawRectangleRec(rowRect, ColorAlpha(theme.panelBg, 0.25f));

        float indent = (float)row.depth * 12.0f;
        float arrowX = body.x + 8.0f + indent;
        float cy = y + kRowHeight / 2.0f;
        Color arrowColor = theme.textSecondary;
        if (row.hasChildren) {
            if (row.expanded) {
                DrawTriangle({ arrowX - 3, cy - 2 }, { arrowX, cy + 3 }, { arrowX + 3, cy - 2 }, arrowColor);
            } else {
                DrawTriangle({ arrowX - 2, cy - 4 }, { arrowX - 2, cy + 4 }, { arrowX + 3, cy }, arrowColor);
            }
        }

        float nameX = arrowX + 9.0f;
        if (!row.synthetic) {
            Rectangle swatch = { nameX, cy - 4.0f, 8.0f, 8.0f };
            bool drawn = drawnLines.count(row.key) > 0;
            Color swatchColor = LineColor(row.key);
            if (drawn) DrawRectangleRec(swatch, swatchColor);
            else DrawRectangleLinesEx(swatch, 1, ColorAlpha(theme.textSecondary, 0.7f));
            if (Hovered(swatch) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                swatchClicked = true;
                swatchKey = row.key;
            }
            nameX += 13.0f;
        }

        Color textColor = (row.isWait || row.key == ProfilerModel::kKeyUnaccounted) ? theme.textSecondary : theme.textPrimary;
        if (row.synthetic && !row.isWait && row.key != ProfilerModel::kKeyUnaccounted) textColor = theme.accent;
        DrawText(ClipText(row.name, nameRight - nameX).c_str(), (int)nameX, (int)(y + 3), 10, textColor);

        DrawRightAligned(Duration(row.totalNs), colSelf - 4.0f, y + 3, textColor);
        DrawRightAligned(row.synthetic ? std::string("-") : Duration(row.selfNs), colPct - 4.0f, y + 3, theme.textSecondary);
        DrawRightAligned(Percent(row.percentOfWork), colAvg - 4.0f, y + 3, theme.textSecondary);
        DrawRightAligned(row.key == ProfilerModel::kKeyUnaccounted ? std::string("-") : DurationUs(row.avgUs), colMax - 4.0f, y + 3, theme.textSecondary);
        DrawRightAligned(row.key == ProfilerModel::kKeyUnaccounted ? std::string("-") : DurationUs(row.maxUs), colCalls - 4.0f, y + 3, theme.textSecondary);
        DrawRightAligned(std::to_string(row.calls), right - 4.0f, y + 3, theme.textSecondary);

        if (rowHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) toggleRow = (int)i;
    }
    EndScissorMode();

    if (maxScroll > 0.0f) {
        Rectangle track = { area.x + area.width - scrollbarW - 1.0f, body.y, scrollbarW, body.height };
        DrawRectangleRec(track, ColorAlpha(theme.panelBorder, 0.35f));
        float thumbH = std::max(16.0f, track.height * (body.height / contentH));
        float thumbY = track.y + (track.height - thumbH) * (tableScroll / maxScroll);
        DrawRectangleRec({ track.x, thumbY, track.width, thumbH }, ColorAlpha(theme.textSecondary, 0.6f));
    }

    if (swatchClicked) {
        if (drawnLines.count(swatchKey)) {
            pinned.erase(swatchKey);
            hidden.insert(swatchKey);
        } else {
            hidden.erase(swatchKey);
            pinned.insert(swatchKey);
        }
    } else if (toggleRow >= 0) {
        const ProfilerModel::Row& row = rows[(size_t)toggleRow];
        selectedKey = row.key;
        if (row.hasChildren) {
            if (expanded.count(row.key)) expanded.erase(row.key);
            else expanded.insert(row.key);
        }
    }
}
