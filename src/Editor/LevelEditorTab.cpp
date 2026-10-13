#include "LevelEditorTab.h"
#include "ViewportPanel.h"
#include "DetailsPanel.h"
#include "ActorDetailsBuilder.h"
#include "OutlinerPanel.h"
#include "ContentBrowserPanel.h"
#include "LogPanel.h"
#include "ModuleBrowserPanel.h"
#include "ProfilerPanel.h"
#include "PlaceActorsPanel.h"
#include "MenuBar.h"
#include "EditorUndo.h"
#include "Core/LayoutSerializer.h"
#include <cstdlib>
#include "NativeFileDialog.h"
#include <filesystem>

LevelEditorTab::~LevelEditorTab() {
    if (undoDocument) {
        UndoManager::Get().UnregisterDocument(undoDocument);
        delete undoDocument;
        undoDocument = nullptr;
    }
}

bool LevelEditorTab::IsDirty() const {
    return viewportPanel && viewportPanel->IsMapDirty();
}

void LevelEditorTab::Initialize() {
    title     = "Level Editor";
    permanent = true;

    viewportPanel    = new ViewportPanel();
    detailsPanel     = new DetailsPanel();
    outlinerPanel    = new OutlinerPanel();
    contentPanel     = new ContentBrowserPanel();
    logPanel         = new LogPanel();
    modulePanel      = new ModuleBrowserPanel();
    placeActorsPanel = new PlaceActorsPanel();
    profilerPanel    = new ProfilerPanel();

    viewportPanel->Initialize();
    detailsPanel->Initialize();
    outlinerPanel->Initialize();
    contentPanel->Initialize();
    logPanel->Initialize();
    modulePanel->Initialize();
    placeActorsPanel->Initialize();
    profilerPanel->Initialize();

    modulePanel->SetLogPanel(logPanel);
    viewportPanel->SetLogPanel(logPanel);

    undoDocument = new FunctionUndoDocument();
    undoDocument->name = "Level Edit";
    undoDocument->tab = this;
    undoDocument->capture = [this](std::string& content, std::string& selection) {
        viewportPanel->CaptureUndoState(content, selection);
    };
    undoDocument->restore = [this](const std::string& content, const std::string& selection) {
        viewportPanel->RestoreUndoState(content, selection);
    };
    undoDocument->canTrack = [this]() { return viewportPanel->CanTrackUndo(); };
    viewportPanel->onMapReplaced = [this]() { UndoManager::Get().ForgetHistory(undoDocument); };
    UndoManager::Get().RegisterDocument(undoDocument);

    // Wire viewport -> details and outliner selection pointers.
    detailsPanel->buildRows = [this]() -> std::vector<PropertyRow> {
        const StarMap* map = viewportPanel->GetStarMap();
        int* idx = viewportPanel->GetSelectedActorIndexPtr();
        if (!map || !idx || *idx < 0 || *idx >= (int)map->GetInstances().size()) return {};
        ActorInstance& inst = const_cast<ActorInstance&>(map->GetInstances()[*idx]);

        std::function<ActorInstance*()> currentInstance = [this]() -> ActorInstance* {
            const StarMap* m = viewportPanel->GetStarMap();
            int* i = viewportPanel->GetSelectedActorIndexPtr();
            if (!m || !i || *i < 0 || *i >= (int)m->GetInstances().size()) return nullptr;
            return const_cast<ActorInstance*>(&m->GetInstances()[*i]);
        };
        std::function<void()> onEdited = [this]() { viewportPanel->MarkDirty(); };
        std::function<void(const std::string&, const std::string&)> onOpenAsset =
            [this](const std::string& path, const std::string& ext) {
                if (onAssetOpened) onAssetOpened(path, ext);
            };
        return BuildActorPropertyRows(inst, currentInstance, onEdited, onOpenAsset);
    };
    detailsPanel->onAssetPicked = [this](const std::string& rowId, const std::string& pickedPath) {
        const StarMap* map = viewportPanel->GetStarMap();
        int* idx = viewportPanel->GetSelectedActorIndexPtr();
        if (!map || !idx || *idx < 0 || *idx >= (int)map->GetInstances().size()) return;
        ActorInstance& inst = const_cast<ActorInstance&>(map->GetInstances()[*idx]);
        if (rowId == "mesh") {
            inst.meshAssetPath = pickedPath;
        } else if (rowId.rfind("material:", 0) == 0) {
            int slot = atoi(rowId.c_str() + 9);
            if (slot >= 0 && slot < (int)inst.materialAssetPaths.size()) inst.materialAssetPaths[slot] = pickedPath;
        } else if (rowId == "sound") {
            inst.component.fields["Sound"] = pickedPath;
        }
        viewportPanel->MarkDirty();
    };
    viewportPanel->onToggleFullscreen = [this]() {
        dockArea.ToggleMaximizedPanel("Viewport");
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

    placeActorsPanel->onActorDragged = [this](const BasicActorDef& def, Vector2 dropPos) {
        viewportPanel->DropBasicActor(def, dropPos);
    };

    dockArea.SetPanels({
        viewportPanel, detailsPanel, outlinerPanel, contentPanel, logPanel, modulePanel, placeActorsPanel, profilerPanel
    });

    BuildDefaultLayout();
}

void LevelEditorTab::BuildDefaultLayout() {

    auto placeActorsLeaf = std::make_unique<DockNode>();
    placeActorsLeaf->panelNames = { "Place Actors" };
    placeActorsLeaf->activeTab  = 0;

    auto viewportLeaf = std::make_unique<DockNode>();
    viewportLeaf->panelNames = { "Viewport" };
    viewportLeaf->activeTab  = 0;

    auto topRow = std::make_unique<DockNode>();
    topRow->type       = DockNode::Type::SplitH;
    topRow->splitRatio = 0.2230f;
    topRow->first      = std::move(placeActorsLeaf);
    topRow->second     = std::move(viewportLeaf);

    auto bottomLeaf = std::make_unique<DockNode>();
    bottomLeaf->panelNames = { "Content Browser", "Logs", "Modules", "Profiler" };
    bottomLeaf->activeTab  = 0;

    auto leftCol = std::make_unique<DockNode>();
    leftCol->type       = DockNode::Type::SplitV;
    leftCol->splitRatio = 0.70f;
    leftCol->first      = std::move(topRow);
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
    root->first       = std::move(leftCol);
    root->second      = std::move(rightCol);

    dockArea.SetDefaultLayout(std::move(root));
}

void LevelEditorTab::Render(float x, float y, float width, float height) {
    // Sync starmap pointers each frame in case the viewport loaded a new map.
    outlinerPanel->starMap            = viewportPanel->GetStarMap();
    outlinerPanel->selectedActorIndex = viewportPanel->GetSelectedActorIndexPtr();
    viewportPanel->isFullscreen = dockArea.IsPanelMaximized("Viewport");

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
        dockArea.IsPanelVisible("Place Actors"),
        dockArea.IsPanelVisible("Profiler"),
        false, false, false, false,
    };

    menuBar->SetMenus({
        { "File",   { "New Project", "Open Project", "Save Project" } },
        { "Edit",   { "Editor Settings", "Project Settings" } },
        { "Window", { "Viewport", "Details", "Outliner", "Content Browser", "Logs", "Modules", "Place Actors", "Profiler", "---", "Reset Layout", "Save Layout As", "Load Layout" }, windowChecked },
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
    for (const std::string& name : { "Viewport", "Details", "Outliner", "Content Browser", "Logs", "Modules", "Place Actors", "Profiler" })
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
