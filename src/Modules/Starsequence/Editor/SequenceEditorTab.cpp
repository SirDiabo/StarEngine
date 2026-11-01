#include "SequenceEditorTab.h"
#include "SequenceEditorSession.h"
#include "SequencePanels.h"
#include "PianoRollView.h"
#include "Editor/LogPanel.h"
#include "Editor/MenuBar.h"
#include "Editor/EditorUndo.h"
#include "Editor/UITheme.h"
#include "Core/LayoutSerializer.h"
#include "Core/KeyLayout.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/WidgetFont.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

constexpr float kToolbarHeight = 30.0f;

bool ToolbarButton(Rectangle r, const char* label, bool active) {
    const UIPalette& theme = UITheme::Get();
    bool hovered = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, active ? theme.accent : (hovered ? theme.buttonHover : theme.buttonBg));
    DrawRectangleLinesEx(r, 1, theme.buttonBorder);
    Font font = WidgetFont::Get();
    float tw = MeasureTextEx(font, label, 11, 1.0f).x;
    DrawTextEx(font, label, { r.x + (r.width - tw) / 2.0f, r.y + (r.height - 11.0f) / 2.0f }, 11, 1.0f, theme.textPrimary);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

}

SequenceEditorTab::SequenceEditorTab(const std::string& path) : assetPath(path) {
    title = fs::path(path).stem().string();
    permanent = false;
}

SequenceEditorTab::~SequenceEditorTab() {
    delete pianoRollPanel;
    delete layersPanel;
    delete session;
}

void SequenceEditorTab::Initialize() {
    session = new SequenceEditorSession();
    session->SetUndoOwner([this]() { EditorUndo::RevealTab(this); }, [this]() { return EditorUndo::IsTabActive(this); });
    opened = session->Open(assetPath);

    pianoRollPanel = new PianoRollPanel();
    layersPanel = new SequenceLayersPanel();
    pianoRollPanel->Initialize();
    layersPanel->Initialize();
    pianoRollPanel->SetSession(session);
    layersPanel->SetSession(session);

    std::vector<Panel*> panels = { pianoRollPanel, layersPanel };
    if (logPanel) panels.push_back(logPanel);
    dockArea.SetPanels(panels);

    BuildDefaultLayout();
}

void SequenceEditorTab::BuildDefaultLayout() {
    auto layersLeaf = std::make_unique<DockNode>();
    layersLeaf->panelNames = { "Layers" };

    auto rollLeaf = std::make_unique<DockNode>();
    rollLeaf->panelNames = { "Piano Roll" };

    std::unique_ptr<DockNode> mainSide;
    if (logPanel) {
        auto logsLeaf = std::make_unique<DockNode>();
        logsLeaf->panelNames = { "Logs" };
        mainSide = std::make_unique<DockNode>();
        mainSide->type = DockNode::Type::SplitV;
        mainSide->splitRatio = 0.8f;
        mainSide->first = std::move(rollLeaf);
        mainSide->second = std::move(logsLeaf);
    } else {
        mainSide = std::move(rollLeaf);
    }

    auto root = std::make_unique<DockNode>();
    root->type = DockNode::Type::SplitH;
    root->splitRatio = 0.22f;
    root->first = std::move(layersLeaf);
    root->second = std::move(mainSide);

    dockArea.SetDefaultLayout(std::move(root));
}

void SequenceEditorTab::RenderToolbar(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    Font font = WidgetFont::Get();
    DrawRectangleRec(area, theme.panelBg);
    DrawLineEx({ area.x, area.y + area.height }, { area.x + area.width, area.y + area.height }, 1.0f, theme.panelBorder);

    float x = area.x + 6.0f;
    float y = area.y + 4.0f;
    float h = area.height - 8.0f;

    if (ToolbarButton({ x, y, 54, h }, "Save", false)) session->Save();
    x += 62.0f;

    bool playing = session->IsPlaying();
    if (ToolbarButton({ x, y, 64, h }, playing ? "Stop" : "Play", playing)) session->TogglePlay();
    x += 72.0f;

    if (ToolbarButton({ x, y, 54, h }, "Loop", session->loopEnabled)) session->loopEnabled = !session->loopEnabled;
    x += 66.0f;

    const Starsequence::Sequence& seq = session->GetSequence();
    double tick = session->PlayheadTick();
    int bar = 1, beat = 1;
    Starsequence::TickToBarBeat(seq, (int64_t)tick, bar, beat);
    double seconds = seq.TickToSeconds(tick);
    char text[160];
    snprintf(text, sizeof(text), "%d.%d    %d:%05.2f    %.1f BPM    %d voices",
             bar, beat, (int)(seconds / 60.0), seconds - 60.0 * (double)((int)(seconds / 60.0)), seq.BpmAt((int64_t)tick), session->ActiveVoices());
    DrawTextEx(font, text, { x, area.y + (area.height - 11.0f) / 2.0f }, 11, 1.0f, theme.textPrimary);
}

void SequenceEditorTab::Render(float x, float y, float width, float height) {
    if (!session || !session->IsOpen()) return;

    bool active = EditorUndo::IsTabActive(this);
    bool typing = WidgetInput::IsKeyboardCaptured() || WidgetInput::HasModal(WidgetInput::kEditorDevice);
    if (active && !typing) {
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (ctrl && KeyLayout::Pressed('s')) session->Save();
        else if (!ctrl && IsKeyPressed(KEY_SPACE)) session->TogglePlay();
    }

    session->Update();
    RenderToolbar({ x, y, width, kToolbarHeight });
    dockArea.Render({ x, y + kToolbarHeight, width, height - kToolbarHeight });
}

void SequenceEditorTab::RenderOverlays() {
    dockArea.RenderOverlays();
}

void SequenceEditorTab::BuildMenus(MenuBar* menuBar) {
    std::vector<bool> windowChecked = {
        dockArea.IsPanelVisible("Piano Roll"),
        dockArea.IsPanelVisible("Layers"),
        dockArea.IsPanelVisible("Logs"),
        false, false,
    };

    menuBar->SetMenus({
        { "File",   { "Save", "Save All" } },
        { "Asset",  { "Find in Content Browser" } },
        { "Window", { "Piano Roll", "Layers", "Logs", "---", "Reset Layout" }, windowChecked },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}

void SequenceEditorTab::OnDeactivated() {
    if (session) session->Stop();
}

bool SequenceEditorTab::IsOpen() const {
    return session && opened && session->IsOpen();
}

bool SequenceEditorTab::IsDirty() const {
    return session && session->IsDirty();
}

void SequenceEditorTab::ResetLayout() {
    for (const std::string& name : { "Piano Roll", "Layers", "Logs" })
        dockArea.SetPanelVisible(name, true);
    BuildDefaultLayout();
}

void SequenceEditorTab::TogglePanelVisible(const std::string& name) {
    bool current = dockArea.IsPanelVisible(name);
    dockArea.SetPanelVisible(name, !current);
}

void SequenceEditorTab::SaveLayout(const std::string& filePath) const {
    std::unordered_map<std::string, std::string> data;
    LayoutSerializer::Load(filePath, data);
    dockArea.SaveLayout(data, "starsequence");
    LayoutSerializer::Save(filePath, data);
}

void SequenceEditorTab::LoadLayout(const std::string& filePath) {
    std::unordered_map<std::string, std::string> data;
    if (LayoutSerializer::Load(filePath, data))
        dockArea.LoadLayout(data, "starsequence");
}
