#include "LevelEditorTab.h"
#include "ViewportPanel.h"
#include "DetailsPanel.h"
#include "OutlinerPanel.h"
#include "ContentBrowserPanel.h"
#include "LogPanel.h"
#include "ModuleBrowserPanel.h"
#include "MenuBar.h"
#include "Core/LayoutSerializer.h"
#include "NativeFileDialog.h"
#include <filesystem>

void LevelEditorTab::Initialize() {
    title     = "Level Editor";
    permanent = true;

    viewportPanel  = new ViewportPanel();
    detailsPanel   = new DetailsPanel();
    outlinerPanel  = new OutlinerPanel();
    contentPanel   = new ContentBrowserPanel();
    logPanel       = new LogPanel();
    modulePanel    = new ModuleBrowserPanel();

    viewportPanel->Initialize();
    detailsPanel->Initialize();
    outlinerPanel->Initialize();
    contentPanel->Initialize();
    logPanel->Initialize();
    modulePanel->Initialize();

    modulePanel->SetLogPanel(logPanel);
    viewportPanel->SetLogPanel(logPanel);

    // Wire viewport -> details and outliner selection pointers.
    detailsPanel->starMap            = viewportPanel->GetStarMap();
    detailsPanel->selectedActorIndex = viewportPanel->GetSelectedActorIndexPtr();
    detailsPanel->onOpenAsset = [this](const std::string& path, const std::string& ext) {
        if (onAssetOpened) onAssetOpened(path, ext);
    };
    outlinerPanel->starMap            = viewportPanel->GetStarMap();
    outlinerPanel->selectedActorIndex = viewportPanel->GetSelectedActorIndexPtr();

    contentPanel->onAssetOpened = [this](const std::string& path, const std::string& ext) {
        if (onAssetOpened) onAssetOpened(path, ext);
    };

    contentPanel->onAssetDragged = [this](const std::string& path, const std::string& ext, Vector2 dropPos) {
        if (ext == ".staractor" || ext == ".stargraph")
            viewportPanel->DropActorAsset(path, dropPos);
        else if (ext == ".starasset")
            viewportPanel->DropStaticMeshAsset(path, dropPos);
    };

    dockArea.SetPanels({
        viewportPanel, detailsPanel, outlinerPanel, contentPanel, logPanel, modulePanel
    });

    BuildDefaultLayout();
}

void LevelEditorTab::BuildDefaultLayout() {
    // Layout from user spec (level.cfg):
    // Root splits vertically at 0.75: left column (0) and right column (1).
    // Left column (0) splits vertically at 0.70: Viewport (0.0) on top, Content+Logs (0.1) on bottom.
    // Right column (1) splits vertically at 0.333: Outliner (1.0) on top, Details (1.1) on bottom.

    auto viewportLeaf = std::make_unique<DockNode>();
    viewportLeaf->panelNames = { "Viewport" };
    viewportLeaf->activeTab  = 0;

    auto bottomLeaf = std::make_unique<DockNode>();
    bottomLeaf->panelNames = { "Content Browser", "Logs", "Modules" };
    bottomLeaf->activeTab  = 0;

    auto leftCol = std::make_unique<DockNode>();
    leftCol->type       = DockNode::Type::SplitV;
    leftCol->splitRatio = 0.70f;
    leftCol->first      = std::move(viewportLeaf);
    leftCol->second     = std::move(bottomLeaf);

    auto outlinerLeaf = std::make_unique<DockNode>();
    outlinerLeaf->panelNames = { "Outliner" };
    outlinerLeaf->activeTab  = 0;

    auto detailsLeaf = std::make_unique<DockNode>();
    detailsLeaf->panelNames = { "Details" };
    detailsLeaf->activeTab  = 0;

    auto rightCol = std::make_unique<DockNode>();
    rightCol->type       = DockNode::Type::SplitV;
    rightCol->splitRatio = 0.333f;
    rightCol->first      = std::move(outlinerLeaf);
    rightCol->second     = std::move(detailsLeaf);

    auto root = std::make_unique<DockNode>();
    root->type       = DockNode::Type::SplitH;
    root->splitRatio = 0.75f;
    root->first      = std::move(leftCol);
    root->second     = std::move(rightCol);

    dockArea.SetDefaultLayout(std::move(root));
}

void LevelEditorTab::Render(float x, float y, float width, float height) {
    // Sync starmap pointers each frame in case the viewport loaded a new map.
    detailsPanel->starMap            = viewportPanel->GetStarMap();
    detailsPanel->selectedActorIndex = viewportPanel->GetSelectedActorIndexPtr();
    outlinerPanel->starMap            = viewportPanel->GetStarMap();
    outlinerPanel->selectedActorIndex = viewportPanel->GetSelectedActorIndexPtr();

    // Update tab title to reflect the currently open map.
    if (viewportPanel->IsMapLoaded()) {
        std::filesystem::path p(viewportPanel->GetMapPath());
        title = p.stem().string();
    } else {
        title = "Level Editor";
    }

    dockArea.Render({ x, y, width, height });
}

void LevelEditorTab::RenderOverlays() {
    dockArea.RenderOverlays();
}

void LevelEditorTab::BuildMenus(MenuBar* menuBar) {
    std::vector<bool> windowChecked = {
        dockArea.IsPanelVisible("Viewport"),
        dockArea.IsPanelVisible("Details"),
        dockArea.IsPanelVisible("Outliner"),
        dockArea.IsPanelVisible("Content Browser"),
        dockArea.IsPanelVisible("Logs"),
        dockArea.IsPanelVisible("Modules"),
        false, false, false, false,
    };

    menuBar->SetMenus({
        { "File",   { "New Project", "Open Project", "Save Project" } },
        { "Edit",   { "Editor Settings", "Project Settings" } },
        { "Window", { "Viewport", "Details", "Outliner", "Content Browser", "Logs", "Modules", "---", "Reset Layout", "Save Layout As", "Load Layout" }, windowChecked },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}

void LevelEditorTab::SaveLayout(const std::string& filePath) const {
    std::unordered_map<std::string, std::string> data;
    LayoutSerializer::Load(filePath, data);
    dockArea.SaveLayout(data, "level");
    LayoutSerializer::Save(filePath, data);
}

void LevelEditorTab::LoadLayout(const std::string& filePath) {
    std::unordered_map<std::string, std::string> data;
    if (LayoutSerializer::Load(filePath, data))
        dockArea.LoadLayout(data, "level");
}

void LevelEditorTab::ResetLayout() {
    for (const std::string& name : { "Viewport", "Details", "Outliner", "Content Browser", "Logs", "Modules" })
        dockArea.SetPanelVisible(name, true);
    BuildDefaultLayout();
}

void LevelEditorTab::SaveLayoutAs() {
    std::string dir = (std::filesystem::path(getenv("USERPROFILE") ? getenv("USERPROFILE") :
                        (getenv("HOME") ? getenv("HOME") : ".")) / ".starengine").string();
    std::string path = NativeFileDialog::SaveFile(
        "Save Layout", dir, "my_layout.cfg", "Layout Files", "*.cfg");
    if (!path.empty()) {
        if (path.size() < 4 || path.substr(path.size() - 4) != ".cfg")
            path += ".cfg";
        SaveLayout(path);
    }
}

void LevelEditorTab::LoadLayoutFromDialog() {
    std::string dir = (std::filesystem::path(getenv("USERPROFILE") ? getenv("USERPROFILE") :
                        (getenv("HOME") ? getenv("HOME") : ".")) / ".starengine").string();
    std::string path = NativeFileDialog::OpenFile(
        "Load Layout", dir, "Layout Files", "*.cfg");
    if (!path.empty())
        LoadLayout(path);
}

void LevelEditorTab::TogglePanelVisible(const std::string& name) {
    bool current = dockArea.IsPanelVisible(name);
    dockArea.SetPanelVisible(name, !current);
}

void LevelEditorTab::SetPackageCallback(std::function<void()> callback) {
    if (viewportPanel) viewportPanel->SetPackageCallback(callback);
}

bool LevelEditorTab::HasUnsavedLevelChanges() const {
    return viewportPanel && viewportPanel->IsMapLoaded() && viewportPanel->IsMapDirty();
}

void LevelEditorTab::SaveLevel() {
    if (viewportPanel) viewportPanel->SaveMap();
}