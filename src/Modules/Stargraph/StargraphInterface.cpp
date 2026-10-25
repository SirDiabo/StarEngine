#include "StargraphInterface.h"
#include "StargraphBehavior.h"
#include "Runtime/WidgetBehavior.h"

#ifndef STARENGINE_GAME_BUILD
#include "Core/DockNode.h"
#include "Editor/StargraphEditorPanel.h"
#include "Editor/StargraphEditorTab.h"
#include "Core/EditorAssetHandlerRegistry.h"
#include "Editor/StargraphPanel.h"
#include "Editor/StargraphGraphPanel.h"
#include "Editor/StargraphDetailsPanel.h"
#endif

void RegisterStargraphInterface() {
    ProgrammingInterfaceInfo info;
    info.name = "Stargraph";
    info.displayLabel = "Graph";
    info.moduleName = "Stargraph";
    info.legacyActorSection = "Stargraph";

    info.createDefaultContent = []() -> IBehaviorContent* {
        auto* content = new StargraphBehaviorContent();
        content->graph.parentClass = "Widget";
        return content;
    };

    info.saveContent = [](std::ostream& out, const IBehaviorContent& content, const std::string& prefix) {
        static_cast<const StargraphBehaviorContent&>(content).graph.SaveGraphContent(out, prefix);
    };

    info.loadContent = [](const std::vector<SectionFileIO::ParsedSection>& sections, const std::string& prefix) -> IBehaviorContent* {
        auto* content = new StargraphBehaviorContent();
        content->graph.LoadGraphContent(sections, prefix);
        return content;
    };

    info.createWidgetBehavior = [](IBehaviorContent& content) -> WidgetBehavior* {
        return new StargraphBehavior(static_cast<StargraphBehaviorContent&>(content).graph);
    };

    info.createActorContent = []() -> IBehaviorContent* {
        auto* content = new StargraphBehaviorContent();
        content->graph.parentClass = "Actor";
        return content;
    };

    info.saveActorContent = [](std::ostream& out, const IBehaviorContent& content, const std::string& prefix) {
        static_cast<const StargraphBehaviorContent&>(content).graph.SaveContent(out, prefix);
    };

    info.loadActorContent = [](const std::vector<SectionFileIO::ParsedSection>& sections, const std::string& prefix) -> IBehaviorContent* {
        auto* content = new StargraphBehaviorContent();
        content->graph.LoadContent(sections, prefix);
        content->graph.parentClass = "Actor";
        return content;
    };

    info.createActorBehavior = [](IBehaviorContent& content, const std::vector<ComponentDecl>&) -> ActorBehavior* {
        return new StargraphBehavior(static_cast<StargraphBehaviorContent&>(content).graph);
    };

#ifndef STARENGINE_GAME_BUILD
    info.createActorEditorTab = [](const std::string& fullPath, const EditorServices& services) -> EditorTab* {
        auto* tab = new StargraphEditorTab(fullPath);
        tab->SetSharedLogPanel(services.sharedLogPanel);
        tab->SetViewportPanel(services.sharedViewportPanel);
        return tab;
    };

    info.buildWidgetEditorPanels = [](const ProgrammingInterfaceContext& ctx) -> ProgrammingInterfacePanelSet {
        auto* content = static_cast<StargraphBehaviorContent*>(ctx.content);

        auto* session = new StargraphEditorPanel();
        session->Initialize();
        session->SetViewportPanel(ctx.sharedViewportPanel);
        session->SetWidgetElementLister(ctx.listElements);
        session->SetUndoOwner(ctx.revealOwnerTab, ctx.isOwnerTabActive);
        session->OpenEmbedded(content->graph, ctx.assetKind);

        auto* stargraphPanel = new StargraphPanel();
        auto* graphPanel = new StargraphGraphPanel();
        auto* detailsPanel = new StargraphDetailsPanel();
        stargraphPanel->Initialize();
        graphPanel->Initialize();
        detailsPanel->Initialize();
        stargraphPanel->SetSession(session);
        graphPanel->SetSession(session);
        detailsPanel->SetSession(session);

        ProgrammingInterfacePanelSet result;
        result.panels = { stargraphPanel, graphPanel, detailsPanel };
        result.detailsPanel = detailsPanel;
        result.listPanelName = "My Stargraph";
        result.mainPanelName = "Graph";
        result.detailsSelectionKey = [session]() { return session->DetailsSelectionKey(); };

        auto graphLeaf = std::make_unique<DockNode>();
        graphLeaf->panelNames = { "Graph" };

        auto varsLeaf = std::make_unique<DockNode>();
        varsLeaf->panelNames = { "My Stargraph" };

        auto detailsLeaf = std::make_unique<DockNode>();
        detailsLeaf->panelNames = { "Details" };

        auto graphAndDetails = std::make_unique<DockNode>();
        graphAndDetails->type = DockNode::Type::SplitH;
        graphAndDetails->splitRatio = 0.8f;
        graphAndDetails->first = std::move(graphLeaf);
        graphAndDetails->second = std::move(detailsLeaf);

        auto root = std::make_unique<DockNode>();
        root->type = DockNode::Type::SplitV;
        root->splitRatio = 0.3f;
        root->first = std::move(varsLeaf);
        root->second = std::move(graphAndDetails);

        result.layout = std::move(root);

        result.syncBeforeSave = [session, content]() {
            content->graph = session->GetGraph();
        };
        result.teardown = [session]() {
            delete session;
        };
        result.onElementDragStarted = [session](const std::string& elementName, const std::string& elementClass) {
            session->BeginElementDrag(elementName, elementClass);
        };

        return result;
    };
#endif

    ProgrammingInterfaceRegistry::Get().RegisterInterface(info);
}
