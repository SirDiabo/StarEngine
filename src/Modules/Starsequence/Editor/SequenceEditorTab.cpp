#include "SequenceEditorTab.h"
#include "SequenceEditorSession.h"
#include "SequencePanels.h"
#include "SequenceDetailsPanel.h"
#include "Sf2ImportPanel.h"
#include "InstrumentEditorPanel.h"
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
    delete instrumentsPanel;
    delete detailsPanel;
    delete sf2Panel;
    delete instrumentEditorPanel;
    delete session;
}

void SequenceEditorTab::Initialize() {
    session = new SequenceEditorSession();
    session->SetUndoOwner([this]() { EditorUndo::RevealTab(this); }, [this]() { return EditorUndo::IsTabActive(this); });
    opened = session->Open(assetPath);

    pianoRollPanel = new PianoRollPanel();
    layersPanel = new SequenceLayersPanel();
    instrumentsPanel = new SequenceInstrumentsPanel();
    detailsPanel = new SequenceDetailsPanel();
    pianoRollPanel->Initialize();
    layersPanel->Initialize();
    instrumentsPanel->Initialize();
    detailsPanel->Initialize();
    pianoRollPanel->SetSession(session);
    layersPanel->SetSession(session);
    instrumentsPanel->SetSession(session);
    detailsPanel->SetSession(session);
    sf2Panel = new Sf2ImportPanel();
    sf2Panel->Initialize();
    sf2Panel->SetSession(session);
    instrumentsPanel->SetOnImportSoundFont([this]() {
        sf2Panel->Open();
        dockArea.OpenFloating("SoundFont Import", { 640.0f, 560.0f });
    });

    instrumentEditorPanel = new InstrumentEditorPanel();
    instrumentEditorPanel->Initialize();
    instrumentEditorPanel->SetSession(session);
    auto openInstrumentEditor = [this]() {
        instrumentEditorPanel->Open();
        dockArea.OpenFloating("Instrument Editor", { 820.0f, 700.0f });
    };
    instrumentsPanel->SetOnEditInstrument(openInstrumentEditor);
    session->onOpenInstrumentEditor = openInstrumentEditor;

    std::vector<Panel*> panels = { pianoRollPanel, layersPanel, instrumentsPanel, detailsPanel, sf2Panel, instrumentEditorPanel };
    if (logPanel) panels.push_back(logPanel);
    dockArea.SetPanels(panels);

    BuildDefaultLayout();
    sf2Panel->SetVisible(false);
    instrumentEditorPanel->SetVisible(false);
}

void SequenceEditorTab::BuildDefaultLayout() {
    auto leaf = [](const char* panelName) {
        auto node = std::make_unique<DockNode>();
        node->panelNames = { panelName };
        return node;
    };

    auto leftColumn = std::make_unique<DockNode>();
    leftColumn->type = DockNode::Type::SplitV;
    leftColumn->splitRatio = 0.55f;
    leftColumn->first = leaf("Layers");
    leftColumn->second = leaf("Instruments");

    std::unique_ptr<DockNode> center;
    if (logPanel) {
        center = std::make_unique<DockNode>();
        center->type = DockNode::Type::SplitV;
        center->splitRatio = 0.8f;
        center->first = leaf("Piano Roll");
        center->second = leaf("Logs");
    } else {
        center = leaf("Piano Roll");
    }

    auto centerAndDetails = std::make_unique<DockNode>();
    centerAndDetails->type = DockNode::Type::SplitH;
    centerAndDetails->splitRatio = 0.78f;
    centerAndDetails->first = std::move(center);
    centerAndDetails->second = leaf("Details");

    auto root = std::make_unique<DockNode>();
    root->type = DockNode::Type::SplitH;
    root->splitRatio = 0.15f;
    root->first = std::move(leftColumn);
    root->second = std::move(centerAndDetails);

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

    {
        char snapText[32];
        snprintf(snapText, sizeof(snapText), "Snap %s", session->SnapLabel());
        Rectangle snapRect = { x, y, 84, h };
        if (ToolbarButton(snapRect, snapText, false)) session->CycleSnap(1);
        else if (!WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), snapRect) && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) session->CycleSnap(-1);
        x += 96.0f;
    }

    {
        struct ToolButton { const char* label; EditTool tool; };
        const ToolButton tools[] = { { "Select", EditTool::Select }, { "Pencil", EditTool::Pencil }, { "Brush", EditTool::Brush } };
        for (const ToolButton& tb : tools) {
            if (ToolbarButton({ x, y, 58, h }, tb.label, session->tool == tb.tool)) session->tool = tb.tool;
            x += 62.0f;
        }
        x += 12.0f;
    }

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
    session->typingInPopup = (sf2Panel && sf2Panel->WantsKeyboard()) || (instrumentEditorPanel && instrumentEditorPanel->WantsKeyboard());
    bool typing = WidgetInput::IsKeyboardCaptured() || WidgetInput::HasModal(WidgetInput::kEditorDevice) || session->typingInPopup;
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
        dockArea.IsPanelVisible("Instruments"),
        dockArea.IsPanelVisible("Details"),
        dockArea.IsPanelVisible("Logs"),
        false, false,
    };

    menuBar->SetMenus({
        { "File",   { "Save", "Save All" } },
        { "Asset",  { "Find in Content Browser" } },
        { "Window", { "Piano Roll", "Layers", "Instruments", "Details", "Logs", "---", "Reset Layout" }, windowChecked },
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
    for (const std::string& name : { "Piano Roll", "Layers", "Instruments", "Details", "Logs" })
        dockArea.SetPanelVisible(name, true);
    BuildDefaultLayout();
    dockArea.SetPanelVisible("SoundFont Import", false);
    dockArea.SetPanelVisible("Instrument Editor", false);
}

void SequenceEditorTab::TogglePanelVisible(const std::string& name) {
    bool current = dockArea.IsPanelVisible(name);
    dockArea.SetPanelVisible(name, !current);
}

void SequenceEditorTab::SaveLayout(const std::string& filePath) const {
    std::unordered_map<std::string, std::string> data;
    LayoutSerializer::Load(filePath, data);
    dockArea.SaveLayout(data, "starsequence_v2");
    LayoutSerializer::Save(filePath, data);
}

void SequenceEditorTab::LoadLayout(const std::string& filePath) {
    std::unordered_map<std::string, std::string> data;
    if (LayoutSerializer::Load(filePath, data))
        dockArea.LoadLayout(data, "starsequence_v2");
}
