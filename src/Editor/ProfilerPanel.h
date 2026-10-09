#pragma once
#include "Core/Panel.h"
#include "Core/Profiler.h"
#include "ProfilerModel.h"
#include <string>
#include <unordered_set>
#include <vector>

class Widget;
class TextElement;

class ProfilerPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~ProfilerPanel() override;

private:
    struct TooltipLine {
        Color color;
        std::string name;
        std::string value;
    };

    struct Tooltip {
        bool active = false;
        uint64_t frameIndex = 0;
        std::vector<TooltipLine> lines;
    };

    void EnsureWidgets();
    void DrawToolbar(Rectangle area);
    void DrawStats(Rectangle area, const Profiler::Frame* frame);
    void DrawGraph(Rectangle area, const std::vector<ProfilerModel::Row>& rows, Tooltip& tooltip);
    void DrawTable(Rectangle area, const std::vector<ProfilerModel::Row>& rows);
    void DrawTooltip(const Tooltip& tooltip);
    const Profiler::Frame* DisplayFrame() const;
    void FreezeFrameAtAge(size_t age);
    void ReturnToLive();
    Color LineColor(uint64_t key);
    std::string LineName(uint64_t key);

    bool recording = true;
    bool autoPie = false;
    bool wasPlayRecording = false;
    bool frozen = false;
    Profiler::Frame frozenFrame;
    int detailLevel = Profiler::DetailStandard;
    int lineCount = 5;
    ProfilerModel::LaneFilter laneFilter = ProfilerModel::LaneFilter::All;

    std::unordered_set<uint64_t> expanded;
    std::unordered_set<uint64_t> pinned;
    std::unordered_set<uint64_t> hidden;
    std::unordered_set<uint64_t> drawnLines;
    uint64_t selectedKey = 0;
    float tableScroll = 0.0f;
    bool initializedExpansion = false;
    ProfilerModel::ColorAllocator colors{ 12 };

    std::vector<Vector2> linePoints;

    Widget* chrome = nullptr;
    TextElement* lineCountField = nullptr;
};
