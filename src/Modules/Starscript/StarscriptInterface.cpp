#include "StarscriptInterface.h"
#include "StarscriptBehavior.h"
#include "StarscriptWidgetBehavior.h"
#include "StarscriptCompiler.h"
#include "Core/SectionFileIO.h"
#include "Core/SafeParse.h"
#include <sstream>
#ifndef STARENGINE_GAME_BUILD
#include "Editor/StarscriptEditorTab.h"
#include "Editor/StarscriptEditorSession.h"
#include "Editor/StarscriptPanels.h"
#include "Editor/DockArea.h"
#include "Core/EditorAssetHandlerRegistry.h"
#endif

namespace Starscript {

const char* const kInterfaceName = "Starscript";

const char* const kDefaultSource =
    "# New Starscript actor\n"
    "\n"
    "event BeginPlay():\n"
    "    print(\"Hello from Starscript\")\n";

const char* const kDefaultWidgetSource =
    "# New Starscript widget\n"
    "#\n"
    "# Name elements in the Hierarchy, then react to them:\n"
    "#   event StartButton.Clicked():\n"
    "#       StatusText.text = \"Started\"\n"
    "\n"
    "event Construct():\n"
    "    pass\n";

namespace {

void SaveSource(std::ostream& out, const IBehaviorContent& base, const std::string& prefix) {
    const StarscriptContent& content = static_cast<const StarscriptContent&>(base);
    std::vector<std::string> lines;
    std::stringstream ss(content.source);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    out << "[" << prefix << "Script]\n";
    out << "LineCount=" << lines.size() << "\n";
    for (size_t n = 0; n < lines.size(); n++) out << "L" << n << "=" << lines[n] << "\n";
    out << "\n";
}

IBehaviorContent* LoadSource(const std::vector<SectionFileIO::ParsedSection>& sections, const std::string& prefix) {
    auto* content = new StarscriptContent();
    for (const SectionFileIO::ParsedSection& section : sections) {
        if (section.name != prefix + "Script") continue;
        int count = 0;
        if (const std::string* v = SectionFileIO::FindEntry(section, "LineCount")) count = SafeParse::ToInt(*v);
        for (int n = 0; n < count; n++) {
            const std::string* v = SectionFileIO::FindEntry(section, "L" + std::to_string(n));
            content->source += (v ? *v : std::string()) + "\n";
        }
        break;
    }
    return content;
}

}

}

void RegisterStarscriptInterface() {
    using namespace Starscript;

    ProgrammingInterfaceInfo info;
    info.name = kInterfaceName;
    info.displayLabel = "Script";
    info.moduleName = "Starscript";

    info.createActorContent = []() -> IBehaviorContent* {
        auto* content = new StarscriptContent();
        content->source = kDefaultSource;
        return content;
    };

    info.saveActorContent = SaveSource;
    info.loadActorContent = LoadSource;

    info.createActorBehavior = [](IBehaviorContent& base, const std::vector<ComponentDecl>& components) -> ActorBehavior* {
        StarscriptContent& content = static_cast<StarscriptContent&>(base);
        if (!content.compiled) content.compiled = Compile(content.source, components);
        return new StarscriptBehavior(content.compiled);
    };

    info.createDefaultContent = []() -> IBehaviorContent* {
        auto* content = new StarscriptContent();
        content->source = kDefaultWidgetSource;
        return content;
    };
    info.saveContent = SaveSource;
    info.loadContent = LoadSource;

    info.createWidgetBehavior = [](IBehaviorContent& base) -> WidgetBehavior* {
        StarscriptContent& content = static_cast<StarscriptContent&>(base);
        if (!content.widgetCache) content.widgetCache = std::make_shared<WidgetCompileCache>();
        return new StarscriptWidgetBehavior(content.source, content.widgetCache);
    };

#ifndef STARENGINE_GAME_BUILD
    info.createActorEditorTab = [](const std::string& fullPath, const EditorServices& services) -> EditorTab* {
        auto* tab = new StarscriptEditorTab(fullPath);
        tab->SetSharedLogPanel(services.sharedLogPanel);
        tab->SetViewportPanel(services.sharedViewportPanel);
        return tab;
    };

    info.buildWidgetEditorPanels = [](const ProgrammingInterfaceContext& ctx) -> ProgrammingInterfacePanelSet {
        auto* content = static_cast<StarscriptContent*>(ctx.content);

        auto* session = new StarscriptEditorSession();
        session->OpenEmbeddedWidget(content, ctx);

        auto* outlinePanel = new StarscriptOutlinePanel();
        auto* scriptPanel = new StarscriptScriptPanel();
        outlinePanel->Initialize();
        scriptPanel->Initialize();
        outlinePanel->SetSession(session);
        scriptPanel->SetSession(session);

        ProgrammingInterfacePanelSet result;
        result.panels = { outlinePanel, scriptPanel };
        result.listPanelName = "Outline";
        result.mainPanelName = "Script";

        auto outlineLeaf = std::make_unique<DockNode>();
        outlineLeaf->panelNames = { "Outline" };
        auto scriptLeaf = std::make_unique<DockNode>();
        scriptLeaf->panelNames = { "Script" };

        auto root = std::make_unique<DockNode>();
        root->type = DockNode::Type::SplitH;
        root->splitRatio = 0.2f;
        root->first = std::move(outlineLeaf);
        root->second = std::move(scriptLeaf);
        result.layout = std::move(root);

        result.syncBeforeSave = [session]() { session->SyncToContent(); };
        result.teardown = [session]() { delete session; };
        result.onElementDragStarted = [session](const std::string& elementName, const std::string& elementClass) {
            session->BeginElementDrag(elementName, elementClass);
        };
        return result;
    };
#endif

    ProgrammingInterfaceRegistry::Get().RegisterInterface(info);
}
