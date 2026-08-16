#include "StargraphEditorTab.h"
#include "StargraphEditorPanel.h"
#include "StargraphOutlinerPanel.h"
#include "StargraphPanel.h"
#include "StargraphGraphPanel.h"
#include "StargraphDetailsPanel.h"
#include "LogPanel.h"
#include "MenuBar.h"
#include "Core/LayoutSerializer.h"
#include "raylib.h"
#include <filesystem>

namespace fs = std::filesystem;

StargraphEditorTab::StargraphEditorTab(const std::string& path)
    : assetPath(path) {
    title     = fs::path(path).stem().string();
    permanent = false;
}

StargraphEditorTab::~StargraphEditorTab() {
    delete outlinerPanel;
    delete stargraphPanel;
    delete graphPanel;
    delete detailsPanel;
    delete panel;
}

void StargraphEditorTab::Initialize() {
    panel = new StargraphEditorPanel();
    panel->Initialize();
    panel->SetViewportPanel(viewportPanel);
    panel->Open(assetPath);

    outlinerPanel  = new StargraphOutlinerPanel();
    stargraphPanel = new StargraphPanel();
    graphPanel     = new StargraphGraphPanel();
    detailsPanel   = new StargraphDetailsPanel();

    outlinerPanel->Initialize();
    stargraphPanel->Initialize();
    graphPanel->Initialize();
    detailsPanel->Initialize();

    outlinerPanel->SetSession(panel);
    stargraphPanel->SetSession(panel);
    graphPanel->SetSession(panel);
    detailsPanel->SetSession(panel);

    std::vector<Panel*> panels = { outlinerPanel, stargraphPanel, graphPanel, detailsPanel };
    if (logPanel) panels.push_back(logPanel);
    dockArea.SetPanels(panels);

    BuildDefaultLayout();
}

void StargraphEditorTab::BuildDefaultLayout() {
    auto outlinerLeaf = std::make_unique<DockNode>();
    outlinerLeaf->panelNames = { "Components" };

    auto stargraphLeaf = std::make_unique<DockNode>();
    stargraphLeaf->panelNames = { "My Stargraph" };

    auto leftCol = std::make_unique<DockNode>();
    leftCol->type       = DockNode::Type::SplitV;
    leftCol->splitRatio = 0.3f;
    leftCol->first       = std::move(outlinerLeaf);
    leftCol->second      = std::move(stargraphLeaf);

    std::unique_ptr<DockNode> graphSide;
    if (logPanel) {
        auto graphLeaf = std::make_unique<DockNode>();
        graphLeaf->panelNames = { "Graph" };

        auto logsLeaf = std::make_unique<DockNode>();
        logsLeaf->panelNames = { "Logs" };

        graphSide = std::make_unique<DockNode>();
        graphSide->type       = DockNode::Type::SplitV;
        graphSide->splitRatio = 0.6912f;
        graphSide->first       = std::move(graphLeaf);
        graphSide->second      = std::move(logsLeaf);
    } else {
        graphSide = std::make_unique<DockNode>();
        graphSide->panelNames = { "Graph" };
    }

    auto detailsLeaf = std::make_unique<DockNode>();
    detailsLeaf->panelNames = { "Details" };

    auto rightSide = std::make_unique<DockNode>();
    rightSide->type       = DockNode::Type::SplitH;
    rightSide->splitRatio = 0.85f;
    rightSide->first       = std::move(graphSide);
    rightSide->second      = std::move(detailsLeaf);

    auto root = std::make_unique<DockNode>();
    root->type       = DockNode::Type::SplitH;
    root->splitRatio = 0.15f;
    root->first       = std::move(leftCol);
    root->second      = std::move(rightSide);

    dockArea.SetDefaultLayout(std::move(root));
}

void StargraphEditorTab::Render(float x, float y, float width, float height) {
    float toolbarH = StargraphEditorPanel::GetToolbarHeight();

    panel->SetScreenBounds({ x, y, width, height });
    panel->RenderToolbar({ x, y, width, toolbarH });

    dockArea.Render({ x, y + toolbarH, width, height - toolbarH });
}

void StargraphEditorTab::RenderOverlays() {
    dockArea.RenderOverlays();
}

void StargraphEditorTab::BuildMenus(MenuBar* menuBar) {
    std::vector<bool> windowChecked = {
        dockArea.IsPanelVisible("Components"),
        dockArea.IsPanelVisible("My Stargraph"),
        dockArea.IsPanelVisible("Graph"),
        dockArea.IsPanelVisible("Details"),
        dockArea.IsPanelVisible("Logs"),
        false, false,
    };

    menuBar->SetMenus({
        { "File",   { "Save", "Save All" } },
        { "Asset",  { "Find in Content Browser" } },
        { "Debug",  { "Play Graph", "Stop" } },
        { "Window", { "Components", "My Stargraph", "Graph", "Details", "Logs", "---", "Reset Layout" }, windowChecked },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}

bool StargraphEditorTab::IsOpen() const {
    return panel && panel->IsOpen();
}

void StargraphEditorTab::ResetLayout() {
    for (const std::string& name : { "Components", "My Stargraph", "Graph", "Details", "Logs" })
        dockArea.SetPanelVisible(name, true);
    BuildDefaultLayout();
}

void StargraphEditorTab::TogglePanelVisible(const std::string& name) {
    bool current = dockArea.IsPanelVisible(name);
    dockArea.SetPanelVisible(name, !current);
}

void StargraphEditorTab::SaveLayout(const std::string& filePath) const {
    std::unordered_map<std::string, std::string> data;
    LayoutSerializer::Load(filePath, data);
    dockArea.SaveLayout(data, "stargraph");
    LayoutSerializer::Save(filePath, data);
}

void StargraphEditorTab::LoadLayout(const std::string& filePath) {
    std::unordered_map<std::string, std::string> data;
    if (LayoutSerializer::Load(filePath, data))
        dockArea.LoadLayout(data, "stargraph");
}
