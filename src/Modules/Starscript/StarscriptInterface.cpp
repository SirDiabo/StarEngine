#include "StarscriptInterface.h"
#include "StarscriptBehavior.h"
#include "StarscriptCompiler.h"
#include "Core/SectionFileIO.h"
#include "Core/SafeParse.h"
#include <sstream>
#ifndef STARENGINE_GAME_BUILD
#include "Editor/StarscriptEditorTab.h"
#include "Core/EditorAssetHandlerRegistry.h"
#endif

namespace Starscript {

const char* const kInterfaceName = "Starscript";

const char* const kDefaultSource =
    "# New Starscript actor\n"
    "\n"
    "event BeginPlay():\n"
    "    print(\"Hello from Starscript\")\n";

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

    info.saveActorContent = [](std::ostream& out, const IBehaviorContent& base, const std::string& prefix) {
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
    };

    info.loadActorContent = [](const std::vector<SectionFileIO::ParsedSection>& sections, const std::string& prefix) -> IBehaviorContent* {
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
    };

    info.createActorBehavior = [](IBehaviorContent& base, const std::vector<ComponentDecl>& components) -> ActorBehavior* {
        StarscriptContent& content = static_cast<StarscriptContent&>(base);
        if (!content.compiled) content.compiled = Compile(content.source, components);
        return new StarscriptBehavior(content.compiled);
    };

#ifndef STARENGINE_GAME_BUILD
    info.createActorEditorTab = [](const std::string& fullPath, const EditorServices& services) -> EditorTab* {
        auto* tab = new StarscriptEditorTab(fullPath);
        tab->SetSharedLogPanel(services.sharedLogPanel);
        tab->SetViewportPanel(services.sharedViewportPanel);
        return tab;
    };
#endif

    ProgrammingInterfaceRegistry::Get().RegisterInterface(info);
}
