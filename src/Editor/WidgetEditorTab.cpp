#include "WidgetEditorTab.h"
#include "WidgetEditorSession.h"
#include "WidgetPalettePanel.h"
#include "WidgetHierarchyPanel.h"
#include "WidgetCanvasPanel.h"
#include "DetailsPanel.h"
#include "Core/Widgets/WidgetObjectElement.h"
#include "WidgetElementDetailsBuilder.h"
#include "Core/Widgets/WidgetElement.h"
#include "Editor/LogPanel.h"
#include "Editor/MenuBar.h"
#include "Editor/EditorUndo.h"
#include "Editor/UITheme.h"
#include "Core/LayoutSerializer.h"
#include "Core/ProgrammingInterfaceRegistry.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/TextElement.h"
#include "raylib.h"
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {

std::unique_ptr<DockNode> CloneDockNode(const DockNode& node) {
    auto copy = std::make_unique<DockNode>();
    copy->type = node.type;
    copy->splitRatio = node.splitRatio;
    copy->panelNames = node.panelNames;
    copy->activeTab = node.activeTab;
    if (node.first) copy->first = CloneDockNode(*node.first);
    if (node.second) copy->second = CloneDockNode(*node.second);
    return copy;
}

}

class WidgetDetailsSwitch : public Panel {
public:
    WidgetDetailsSwitch() { name = "Details"; }

    DetailsPanel* elementDetails = nullptr;
    DetailsPanel* graphDetails = nullptr;
    std::function<std::string()> graphSelectionKey;
    std::function<WidgetElement*()> currentElement;
    bool graphMode = false;

    void Render() override {
        DetailsPanel* active = elementDetails;
        if (graphMode && graphDetails) {
            std::string graphKey = graphSelectionKey ? graphSelectionKey() : std::string();
            WidgetElement* element = currentElement ? currentElement() : nullptr;
            if (element != lastElement) {
                showGraph = false;
                lastElement = element;
            }
            if (graphKey != lastGraphKey) {
                showGraph = !graphKey.empty();
                lastGraphKey = graphKey;
            }
            if (showGraph && graphKey.empty()) showGraph = false;
            if (showGraph) active = graphDetails;
        }
        if (!active) return;
        active->bounds = bounds;
        active->Render();
    }

private:
    bool showGraph = false;
    std::string lastGraphKey;
    WidgetElement* lastElement = nullptr;
};

WidgetEditorTab::WidgetEditorTab(const std::string& path)
    : assetPath(path) {
    title = fs::path(path).stem().string();
    permanent = false;
}

WidgetEditorTab::~WidgetEditorTab() {
    if (undoDocument) {
        UndoManager::Get().UnregisterDocument(undoDocument);
        delete undoDocument;
        undoDocument = nullptr;
    }
    if (graphTeardown) graphTeardown();
    for (Panel* p : graphPanels) delete p;
    delete palettePanel;
    delete hierarchyPanel;
    delete canvasPanel;
    delete detailsSwitch;
    delete detailsPanel;
    delete topBarChrome;
    delete session;
}

void WidgetEditorTab::Initialize() {
    session = new WidgetEditorSession();
    session->Open(assetPath);

    undoDocument = new FunctionUndoDocument();
    undoDocument->name = "Widget Edit";
    undoDocument->tab = this;
    undoDocument->capture = [this](std::string& content, std::string& selection) {
        content = session->CaptureDesign();
        selection = session->SelectedElementId();
    };
    undoDocument->restore = [this](const std::string& content, const std::string& selection) {
        session->RestoreDesign(content, selection);
    };
    undoDocument->canTrack = [this]() { return session && session->IsOpen(); };
    UndoManager::Get().RegisterDocument(undoDocument);

    palettePanel = new WidgetPalettePanel();
    hierarchyPanel = new WidgetHierarchyPanel();
    canvasPanel = new WidgetCanvasPanel();
    detailsPanel = new DetailsPanel();

    palettePanel->Initialize();
    hierarchyPanel->Initialize();
    canvasPanel->Initialize();
    detailsPanel->Initialize();

    palettePanel->SetSession(session);
    hierarchyPanel->SetSession(session);
    canvasPanel->SetSession(session);
    detailsPanel->buildRows = [this]() -> std::vector<PropertyRow> {
        WidgetElement* selected = session ? session->GetSelectedElement() : nullptr;
        if (!selected) return {};
        std::function<WidgetElement*()> currentElement = [this]() -> WidgetElement* {
            return session ? session->GetSelectedElement() : nullptr;
        };
        std::function<void()> onEdited = [this]() { if (session) session->MarkDirty(); };
        return BuildWidgetElementPropertyRows(*selected, currentElement, onEdited);
    };

    detailsPanel->onAssetPicked = [this](const std::string& rowId, const std::string& pickedPath) {
        if (!session || rowId != "widgetAsset") return;
        WidgetObjectElement* object = dynamic_cast<WidgetObjectElement*>(session->GetSelectedElement());
        if (!object) return;
        WidgetObjectElement::BehaviorScope inert(false);
        object->SetWidgetAsset(pickedPath);
        session->MarkDirty();
    };

    detailsSwitch = new WidgetDetailsSwitch();
    detailsSwitch->elementDetails = detailsPanel;
    detailsSwitch->currentElement = [this]() -> WidgetElement* {
        return session ? session->GetSelectedElement() : nullptr;
    };

    std::vector<Panel*> panels = { palettePanel, hierarchyPanel, canvasPanel, detailsSwitch };
    if (logPanel) panels.push_back(logPanel);
    dockArea.SetPanels(panels);

    ApplyDesignerLayout();
    BuildTopBarChrome();
}

void WidgetEditorTab::ApplyDesignerLayout() {
    if (inGraphMode) {
        std::unique_ptr<DockNode> live = dockArea.TakeLayout();
        if (live) graphLayoutSnapshot = std::move(live);
    }
    inGraphMode = false;
    if (detailsSwitch) detailsSwitch->graphMode = false;

    if (designerLayoutSnapshot) {
        dockArea.SetDefaultLayout(CloneDockNode(*designerLayoutSnapshot));
        return;
    }

    auto paletteLeaf = std::make_unique<DockNode>();
    paletteLeaf->panelNames = { "Palette" };

    auto hierarchyLeaf = std::make_unique<DockNode>();
    hierarchyLeaf->panelNames = { "Hierarchy" };

    auto leftCol = std::make_unique<DockNode>();
    leftCol->type = DockNode::Type::SplitV;
    leftCol->splitRatio = 0.45f;
    leftCol->first = std::move(paletteLeaf);
    leftCol->second = std::move(hierarchyLeaf);

    auto canvasLeaf = std::make_unique<DockNode>();
    canvasLeaf->panelNames = { "Canvas" };

    std::unique_ptr<DockNode> centerCol;
    if (logPanel) {
        auto logsLeaf = std::make_unique<DockNode>();
        logsLeaf->panelNames = { "Logs" };

        centerCol = std::make_unique<DockNode>();
        centerCol->type = DockNode::Type::SplitV;
        centerCol->splitRatio = 0.75f;
        centerCol->first = std::move(canvasLeaf);
        centerCol->second = std::move(logsLeaf);
    } else {
        centerCol = std::move(canvasLeaf);
    }

    auto detailsLeaf = std::make_unique<DockNode>();
    detailsLeaf->panelNames = { "Details" };

    auto centerAndRight = std::make_unique<DockNode>();
    centerAndRight->type = DockNode::Type::SplitH;
    centerAndRight->splitRatio = 0.78f;
    centerAndRight->first = std::move(centerCol);
    centerAndRight->second = std::move(detailsLeaf);

    auto root = std::make_unique<DockNode>();
    root->type = DockNode::Type::SplitH;
    root->splitRatio = 0.16f;
    root->first = std::move(leftCol);
    root->second = std::move(centerAndRight);

    designerDefaultLayout = CloneDockNode(*root);
    designerLayoutSnapshot = CloneDockNode(*root);
    dockArea.SetDefaultLayout(std::move(root));
}

bool WidgetEditorTab::ApplyGraphLayout() {
    const ProgrammingInterfaceInfo* provider = nullptr;
    if (!session->GetData().programmingInterface.empty())
        provider = ProgrammingInterfaceRegistry::Get().Find(session->GetData().programmingInterface);

    if (!provider || !provider->buildWidgetEditorPanels) return false;

    if (!graphPanelsBuilt) {
        ProgrammingInterfaceContext ctx;
        ctx.assetKind = "Widget";
        ctx.content = session->GetData().behaviorContent;
        ctx.sharedLogPanel = logPanel;
        ctx.sharedViewportPanel = viewportPanel;
        ctx.revealOwnerTab = [this]() { EditorUndo::RevealTab(this); };
        ctx.isOwnerTabActive = [this]() { return EditorUndo::IsTabActive(this); };
        ctx.onContentDirtied = [this]() { if (session) session->MarkDirty(); };
        ctx.onSaveRequested = [this]() { Save(); };
        ctx.listElements = [this]() {
            std::vector<std::pair<std::string, std::string>> elements;
            std::function<void(WidgetElement*)> walk = [&](WidgetElement* element) {
                if (!element) return;
                elements.emplace_back(element->elementId, element->GetClassName());
                for (WidgetElement* child : element->GetAuthoredChildren()) walk(child);
            };
            walk(session->GetData().rootElement);
            return elements;
        };

        ProgrammingInterfacePanelSet panelSet = provider->buildWidgetEditorPanels(ctx);
        graphPanels = panelSet.panels;
        graphTeardown = panelSet.teardown;
        graphElementDragCallback = panelSet.onElementDragStarted;
        graphModeLabel = provider->displayLabel;
        graphPanelsBuilt = true;

        session->beforeSave = panelSet.syncBeforeSave;
        if (hierarchyPanel) hierarchyPanel->SetElementDragCallback(graphElementDragCallback);

        detailsSwitch->graphDetails = panelSet.detailsPanel;
        detailsSwitch->graphSelectionKey = panelSet.detailsSelectionKey;

        std::vector<Panel*> allPanels = { palettePanel, hierarchyPanel, canvasPanel, detailsSwitch };
        for (Panel* p : graphPanels) {
            if (static_cast<Panel*>(panelSet.detailsPanel) == p) continue;
            allPanels.push_back(p);
        }
        if (logPanel) allPanels.push_back(logPanel);
        dockArea.SetPanels(allPanels);

        auto hierarchyLeaf = std::make_unique<DockNode>();
        hierarchyLeaf->panelNames = { "Hierarchy" };

        auto listLeaf = std::make_unique<DockNode>();
        listLeaf->panelNames = { panelSet.listPanelName };

        auto leftCol = std::make_unique<DockNode>();
        leftCol->type = DockNode::Type::SplitV;
        leftCol->splitRatio = 0.3144f;
        leftCol->first = std::move(hierarchyLeaf);
        leftCol->second = std::move(listLeaf);

        auto graphLeaf = std::make_unique<DockNode>();
        graphLeaf->panelNames = { panelSet.mainPanelName };

        std::unique_ptr<DockNode> graphCol;
        if (logPanel) {
            auto logsLeaf = std::make_unique<DockNode>();
            logsLeaf->panelNames = { "Logs" };

            graphCol = std::make_unique<DockNode>();
            graphCol->type = DockNode::Type::SplitV;
            graphCol->splitRatio = 0.7301f;
            graphCol->first = std::move(graphLeaf);
            graphCol->second = std::move(logsLeaf);
        } else {
            graphCol = std::move(graphLeaf);
        }

        auto detailsLeaf = std::make_unique<DockNode>();
        detailsLeaf->panelNames = { "Details" };

        auto rightCol = std::make_unique<DockNode>();
        rightCol->type = DockNode::Type::SplitH;
        rightCol->splitRatio = 0.8000f;
        rightCol->first = std::move(graphCol);
        rightCol->second = std::move(detailsLeaf);

        auto root = std::make_unique<DockNode>();
        root->type = DockNode::Type::SplitH;
        root->splitRatio = 0.16f;
        root->first = std::move(leftCol);
        root->second = std::move(rightCol);

        graphDefaultLayout = CloneDockNode(*root);
        graphLayoutSnapshot = std::move(root);
    }

    if (!inGraphMode) {
        std::unique_ptr<DockNode> live = dockArea.TakeLayout();
        if (live) designerLayoutSnapshot = std::move(live);
    }

    if (!graphLayoutSnapshot) return false;
    dockArea.SetDefaultLayout(CloneDockNode(*graphLayoutSnapshot));
    inGraphMode = true;
    if (detailsSwitch) detailsSwitch->graphMode = true;
    return true;
}

void WidgetEditorTab::BuildTopBarChrome() {
    delete topBarChrome;
    topBarChrome = new Widget();
    const UIPalette& theme = UITheme::Get();

    auto* root = new WidgetElement();

    auto* saveBtn = new ButtonElement();
    saveBtn->anchor = { 0.0f, 0.0f };
    saveBtn->pivot = { 0.0f, 0.0f };
    saveBtn->offset = { 8.0f, 4.0f };
    saveBtn->size = { 70.0f, 24.0f };
    saveBtn->defaultColor = theme.buttonBg;
    saveBtn->hoveredColor = theme.buttonHover;
    saveBtn->pressedColor = theme.accent;
    saveBtn->onActivated = [this]() { Save(); };

    auto* saveLabel = new TextElement();
    saveLabel->text = "Save";
    saveLabel->color = theme.buttonText;
    saveLabel->fontSize = 13.0f;
    saveLabel->anchor = { 0.5f, 0.5f };
    saveLabel->pivot = { 0.5f, 0.5f };
    saveLabel->size = { 70.0f, 20.0f };
    saveBtn->AddChild(saveLabel);
    root->AddChild(saveBtn);

    const ProgrammingInterfaceInfo* provider = nullptr;
    if (!session->GetData().programmingInterface.empty())
        provider = ProgrammingInterfaceRegistry::Get().Find(session->GetData().programmingInterface);

    if (provider && provider->buildWidgetEditorPanels) {
        auto* toggleBtn = new ButtonElement();
        toggleBtn->anchor = { 1.0f, 0.0f };
        toggleBtn->pivot = { 1.0f, 0.0f };
        toggleBtn->offset = { -8.0f, 4.0f };
        toggleBtn->size = { 110.0f, 24.0f };
        toggleBtn->defaultColor = theme.buttonBg;
        toggleBtn->hoveredColor = theme.buttonHover;
        toggleBtn->pressedColor = theme.accent;

        auto* toggleLabel = new TextElement();
        toggleLabel->text = provider->displayLabel + " ->";
        toggleLabel->color = theme.buttonText;
        toggleLabel->fontSize = 13.0f;
        toggleLabel->anchor = { 0.5f, 0.5f };
        toggleLabel->pivot = { 0.5f, 0.5f };
        toggleLabel->size = { 110.0f, 20.0f };
        toggleBtn->AddChild(toggleLabel);

        toggleBtn->onActivated = [this, toggleLabel]() {
            if (inGraphMode) {
                ApplyDesignerLayout();
                toggleLabel->text = graphModeLabel + " ->";
            } else {
                if (ApplyGraphLayout()) toggleLabel->text = "<- Designer";
            }
        };

        root->AddChild(toggleBtn);
    }

    topBarChrome->SetRoot(root);
}

void WidgetEditorTab::Render(float x, float y, float width, float height) {
    const float toolbarH = 32.0f;
    Rectangle toolbarRect = { x, y, width, toolbarH };

    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(toolbarRect, theme.tabBar);
    DrawLine((int)x, (int)(y + toolbarH), (int)(x + width), (int)(y + toolbarH), theme.panelBorder);

    if (topBarChrome && topBarChrome->rootElement) {
        topBarChrome->rootElement->size = { width, toolbarH };
        float dt = GetFrameTime();
        topBarChrome->Tick(dt, toolbarRect, 1.0f);
        WidgetInput::ProcessInput(topBarChrome, "Editor", dt, GetMousePosition(), 1.0f);
        topBarChrome->Draw(toolbarRect, 1.0f);
    }

    dockArea.Render({ x, y + toolbarH, width, height - toolbarH });
}

void WidgetEditorTab::RenderOverlays() {
    dockArea.RenderOverlays();
}

void WidgetEditorTab::BuildMenus(MenuBar* menuBar) {
    std::vector<bool> windowChecked = {
        dockArea.IsPanelVisible("Palette"),
        dockArea.IsPanelVisible("Hierarchy"),
        dockArea.IsPanelVisible("Canvas"),
        dockArea.IsPanelVisible("Details"),
        dockArea.IsPanelVisible("Logs"),
        false, false,
    };

    menuBar->SetMenus({
        { "File",   { "Save" } },
        { "Asset",  { "Find in Content Browser" } },
        { "Window", { "Palette", "Hierarchy", "Canvas", "Details", "Logs", "---", "Reset Layout" }, windowChecked },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}

bool WidgetEditorTab::IsOpen() const {
    return session && session->IsOpen();
}

bool WidgetEditorTab::IsDirty() const {
    return session && session->IsDirty();
}

void WidgetEditorTab::ResetLayout() {
    for (const std::string& n : { "Palette", "Hierarchy", "Canvas", "Details", "Logs" })
        dockArea.SetPanelVisible(n, true);

    if (inGraphMode) {
        graphLayoutSnapshot = graphDefaultLayout ? CloneDockNode(*graphDefaultLayout) : nullptr;
        ApplyGraphLayout();
    } else {
        designerLayoutSnapshot = designerDefaultLayout ? CloneDockNode(*designerDefaultLayout) : nullptr;
        ApplyDesignerLayout();
    }
}

void WidgetEditorTab::TogglePanelVisible(const std::string& name) {
    bool current = dockArea.IsPanelVisible(name);
    dockArea.SetPanelVisible(name, !current);
}

void WidgetEditorTab::SaveLayout(const std::string& filePath) const {
    std::unordered_map<std::string, std::string> data;
    LayoutSerializer::Load(filePath, data);

    const DockNode* designerTree = inGraphMode ? designerLayoutSnapshot.get() : dockArea.GetLayoutRoot();
    const DockNode* graphTree = inGraphMode ? dockArea.GetLayoutRoot() : graphLayoutSnapshot.get();

    if (designerTree) DockArea::SaveLayoutTree(designerTree, data, "widget.designer");
    if (graphTree) DockArea::SaveLayoutTree(graphTree, data, "widget.graph");
    data["WidgetTab.Mode"] = inGraphMode ? "Graph" : "Designer";

    LayoutSerializer::Save(filePath, data);
}

void WidgetEditorTab::LoadLayout(const std::string& filePath) {
    std::unordered_map<std::string, std::string> data;
    if (!LayoutSerializer::Load(filePath, data)) return;

    std::unique_ptr<DockNode> loadedDesigner = DockArea::LoadLayoutTree(data, "widget.designer");
    std::unique_ptr<DockNode> loadedGraph = DockArea::LoadLayoutTree(data, "widget.graph");

    if (loadedDesigner) designerLayoutSnapshot = std::move(loadedDesigner);

    auto modeIt = data.find("WidgetTab.Mode");
    bool wantGraph = modeIt != data.end() && modeIt->second == "Graph";

    if (wantGraph && ApplyGraphLayout()) {
        if (loadedGraph) {
            graphLayoutSnapshot = std::move(loadedGraph);
            dockArea.SetDefaultLayout(CloneDockNode(*graphLayoutSnapshot));
        }
        return;
    }

    if (loadedGraph) graphLayoutSnapshot = std::move(loadedGraph);
    if (designerLayoutSnapshot) dockArea.SetDefaultLayout(CloneDockNode(*designerLayoutSnapshot));
}

void WidgetEditorTab::Save() {
    if (session) session->Save();
}
