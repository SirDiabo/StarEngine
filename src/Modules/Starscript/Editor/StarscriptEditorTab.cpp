#include "StarscriptEditorTab.h"
#include "StarscriptEditorSession.h"
#include "StarscriptPanels.h"
#include "Editor/ActorClassPanels.h"
#include "Editor/LogPanel.h"
#include "Editor/MenuBar.h"
#include "Editor/EditorUndo.h"
#include "Core/LayoutSerializer.h"
#include "raylib.h"
#include <filesystem>

namespace fs = std::filesystem;

StarscriptEditorTab::StarscriptEditorTab(const std::string& path) : assetPath(path) {
    title = fs::path(path).stem().string();
    permanent = false;
}

StarscriptEditorTab::~StarscriptEditorTab() {
    delete componentsPanel;
    delete outlinePanel;
    delete scriptPanel;
    delete detailsPanel;
    delete previewPanel;
    delete session;
}

void StarscriptEditorTab::Initialize() {
    session = new StarscriptEditorSession();
    session->SetUndoOwner([this]() { EditorUndo::RevealTab(this); }, [this]() { return EditorUndo::IsTabActive(this); });
    opened = session->Open(assetPath);
    session->GetActorEditor().SetOnTogglePreviewFullscreen([this]() { dockArea.ToggleMaximizedPanel("Preview"); });

    componentsPanel = new ComponentOutlinerPanel();
    outlinePanel = new StarscriptOutlinePanel();
    scriptPanel = new StarscriptScriptPanel();
    detailsPanel = new ComponentDetailsPanel();
    previewPanel = new ActorPreviewPanel();

    componentsPanel->Initialize();
    outlinePanel->Initialize();
    scriptPanel->Initialize();
    detailsPanel->Initialize();
    previewPanel->Initialize();

    componentsPanel->SetEditor(&session->GetActorEditor());
    componentsPanel->SetScreenBoundsProvider([this]() { return session->GetScreenBounds(); });
    outlinePanel->SetSession(session);
    scriptPanel->SetSession(session);
    detailsPanel->SetEditor(&session->GetActorEditor());
    previewPanel->SetEditor(&session->GetActorEditor());

    std::vector<Panel*> panels = { componentsPanel, outlinePanel, scriptPanel, detailsPanel, previewPanel };
    if (logPanel) panels.push_back(logPanel);
    dockArea.SetPanels(panels);

    BuildDefaultLayout();
}

void StarscriptEditorTab::BuildDefaultLayout() {
    auto componentsLeaf = std::make_unique<DockNode>();
    componentsLeaf->panelNames = { "Components" };

    auto outlineLeaf = std::make_unique<DockNode>();
    outlineLeaf->panelNames = { "Outline" };

    auto leftCol = std::make_unique<DockNode>();
    leftCol->type = DockNode::Type::SplitV;
    leftCol->splitRatio = 0.3f;
    leftCol->first = std::move(componentsLeaf);
    leftCol->second = std::move(outlineLeaf);

    std::unique_ptr<DockNode> centerSide;
    if (logPanel) {
        auto scriptLeaf = std::make_unique<DockNode>();
        scriptLeaf->panelNames = { "Preview", "Script" };
        scriptLeaf->activeTab = 1;

        auto logsLeaf = std::make_unique<DockNode>();
        logsLeaf->panelNames = { "Logs" };

        centerSide = std::make_unique<DockNode>();
        centerSide->type = DockNode::Type::SplitV;
        centerSide->splitRatio = 0.72f;
        centerSide->first = std::move(scriptLeaf);
        centerSide->second = std::move(logsLeaf);
    } else {
        centerSide = std::make_unique<DockNode>();
        centerSide->panelNames = { "Preview", "Script" };
        centerSide->activeTab = 1;
    }

    auto detailsLeaf = std::make_unique<DockNode>();
    detailsLeaf->panelNames = { "Details" };

    auto rightSide = std::make_unique<DockNode>();
    rightSide->type = DockNode::Type::SplitH;
    rightSide->splitRatio = 0.82f;
    rightSide->first = std::move(centerSide);
    rightSide->second = std::move(detailsLeaf);

    auto root = std::make_unique<DockNode>();
    root->type = DockNode::Type::SplitH;
    root->splitRatio = 0.17f;
    root->first = std::move(leftCol);
    root->second = std::move(rightSide);

    dockArea.SetDefaultLayout(std::move(root));
}

void StarscriptEditorTab::Render(float x, float y, float width, float height) {
    float toolbarH = StarscriptEditorSession::GetToolbarHeight();
    session->SetScreenBounds({ x, y, width, height });
    session->RenderToolbar({ x, y, width, toolbarH });
    session->GetActorEditor().SetPreviewFullscreen(dockArea.IsPanelMaximized("Preview"));
    dockArea.Render({ x, y + toolbarH, width, height - toolbarH });
}

void StarscriptEditorTab::RenderOverlays() {
    dockArea.RenderOverlays();
}

void StarscriptEditorTab::BuildMenus(MenuBar* menuBar) {
    std::vector<bool> windowChecked = {
        dockArea.IsPanelVisible("Preview"),
        dockArea.IsPanelVisible("Components"),
        dockArea.IsPanelVisible("Outline"),
        dockArea.IsPanelVisible("Script"),
        dockArea.IsPanelVisible("Details"),
        dockArea.IsPanelVisible("Logs"),
        false, false,
    };

    menuBar->SetMenus({
        { "File",   { "Save", "Save All" } },
        { "Asset",  { "Find in Content Browser" } },
        { "Window", { "Preview", "Components", "Outline", "Script", "Details", "Logs", "---", "Reset Layout" }, windowChecked },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}

bool StarscriptEditorTab::IsOpen() const {
    return session && opened && session->IsOpen();
}

bool StarscriptEditorTab::IsDirty() const {
    return session && session->IsDirty();
}

void StarscriptEditorTab::ResetLayout() {
    for (const std::string& name : { "Preview", "Components", "Outline", "Script", "Details", "Logs" })
        dockArea.SetPanelVisible(name, true);
    BuildDefaultLayout();
}

void StarscriptEditorTab::TogglePanelVisible(const std::string& name) {
    bool current = dockArea.IsPanelVisible(name);
    dockArea.SetPanelVisible(name, !current);
}

void StarscriptEditorTab::SaveLayout(const std::string& filePath) const {
    std::unordered_map<std::string, std::string> data;
    LayoutSerializer::Load(filePath, data);
    dockArea.SaveLayout(data, "starscript");
    LayoutSerializer::Save(filePath, data);
}

void StarscriptEditorTab::LoadLayout(const std::string& filePath) {
    std::unordered_map<std::string, std::string> data;
    if (LayoutSerializer::Load(filePath, data))
        dockArea.LoadLayout(data, "starscript");
}
