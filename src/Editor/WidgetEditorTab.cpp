#include "WidgetEditorTab.h"
#include "WidgetEditorSession.h"
#include "WidgetPalettePanel.h"
#include "WidgetHierarchyPanel.h"
#include "WidgetCanvasPanel.h"
#include "WidgetDetailsPanel.h"
#include "Editor/LogPanel.h"
#include "Editor/MenuBar.h"
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

WidgetEditorTab::WidgetEditorTab(const std::string& path)
    : assetPath(path) {
    title = fs::path(path).stem().string();
    permanent = false;
}

WidgetEditorTab::~WidgetEditorTab() {
    if (graphTeardown) graphTeardown();
    for (Panel* p : graphPanels) delete p;
    delete palettePanel;
    delete hierarchyPanel;
    delete canvasPanel;
    delete detailsPanel;
    delete topBarChrome;
    delete session;
}

void WidgetEditorTab::Initialize() {
    session = new WidgetEditorSession();
    session->Open(assetPath);

    palettePanel = new WidgetPalettePanel();
    hierarchyPanel = new WidgetHierarchyPanel();
    canvasPanel = new WidgetCanvasPanel();
    detailsPanel = new WidgetDetailsPanel();

    palettePanel->Initialize();
    hierarchyPanel->Initialize();
    canvasPanel->Initialize();
    detailsPanel->Initialize();

    palettePanel->SetSession(session);
    hierarchyPanel->SetSession(session);
    canvasPanel->SetSession(session);
    detailsPanel->SetSession(session);

    std::vector<Panel*> panels = { palettePanel, hierarchyPanel, canvasPanel, detailsPanel };
    if (logPanel) panels.push_back(logPanel);
    dockArea.SetPanels(panels);

    ApplyDesignerLayout();
    BuildTopBarChrome();
}

void WidgetEditorTab::ApplyDesignerLayout() {
    inGraphMode = false;

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

        ProgrammingInterfacePanelSet panelSet = provider->buildWidgetEditorPanels(ctx);
        graphPanels = panelSet.panels;
        graphTeardown = panelSet.teardown;
        graphModeLabel = provider->displayLabel;
        if (panelSet.layout) savedGraphLayout = CloneDockNode(*panelSet.layout);
        graphPanelsBuilt = true;

        session->beforeSave = panelSet.syncBeforeSave;

        std::vector<Panel*> allPanels = { palettePanel, hierarchyPanel, canvasPanel, detailsPanel };
        for (Panel* p : graphPanels) allPanels.push_back(p);
        if (logPanel) allPanels.push_back(logPanel);
        dockArea.SetPanels(allPanels);
    }

    if (!savedGraphLayout) return false;

    auto hierarchyLeaf = std::make_unique<DockNode>();
    hierarchyLeaf->panelNames = { "Hierarchy" };

    auto root = std::make_unique<DockNode>();
    root->type = DockNode::Type::SplitH;
    root->splitRatio = 0.16f;
    root->first = std::move(hierarchyLeaf);
    root->second = CloneDockNode(*savedGraphLayout);

    dockArea.SetDefaultLayout(std::move(root));
    inGraphMode = true;
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

void WidgetEditorTab::ResetLayout() {
    for (const std::string& n : { "Palette", "Hierarchy", "Canvas", "Details", "Logs" })
        dockArea.SetPanelVisible(n, true);
    if (inGraphMode) ApplyGraphLayout();
    else ApplyDesignerLayout();
}

void WidgetEditorTab::TogglePanelVisible(const std::string& name) {
    bool current = dockArea.IsPanelVisible(name);
    dockArea.SetPanelVisible(name, !current);
}

void WidgetEditorTab::SaveLayout(const std::string& filePath) const {
    std::unordered_map<std::string, std::string> data;
    LayoutSerializer::Load(filePath, data);
    dockArea.SaveLayout(data, "widget");
    LayoutSerializer::Save(filePath, data);
}

void WidgetEditorTab::LoadLayout(const std::string& filePath) {
    std::unordered_map<std::string, std::string> data;
    if (LayoutSerializer::Load(filePath, data))
        dockArea.LoadLayout(data, "widget");
}

void WidgetEditorTab::Save() {
    if (session) session->Save();
}
