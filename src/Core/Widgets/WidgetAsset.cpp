#include "WidgetAsset.h"
#include "WidgetAssetCache.h"
#include "Widget.h"
#include "WidgetElementIO.h"
#include "Core/SafeParse.h"
#include "Core/ProgrammingInterfaceRegistry.h"
#include "Runtime/WidgetBehavior.h"
#include <fstream>

namespace WidgetAsset {

bool SaveWidgetAsset(const std::string& path, const WidgetAssetData& data) {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "[Widget]\n";
    out << "Version=1\n";
    out << "Name=" << data.assetName << "\n";
    out << "ParentClass=" << data.parentClass << "\n";
    if (data.uiScale != 1.0f) out << "UIScale=" << data.uiScale << "\n";
    if (data.forceNativeResolution) out << "ForceNativeResolution=true\n";
    if (!data.programmingInterface.empty()) out << "ProgrammingInterface=" << data.programmingInterface << "\n";
    out << "\n";

    WidgetElementIO::WriteElementTree(out, data.rootElement);

    if (!data.programmingInterface.empty() && data.behaviorContent) {
        const ProgrammingInterfaceInfo* provider = ProgrammingInterfaceRegistry::Get().Find(data.programmingInterface);
        if (provider && provider->saveContent) provider->saveContent(out, *data.behaviorContent, "Widget.");
    }

    return true;
}

bool LoadWidgetAsset(const std::string& path, WidgetAssetData& outData) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::vector<SectionFileIO::ParsedSection> sections = SectionFileIO::ParseSections(in);

    for (const SectionFileIO::ParsedSection& section : sections) {
        if (section.name != "Widget") continue;
        if (const std::string* v = SectionFileIO::FindEntry(section, "Name")) outData.assetName = *v;
        if (const std::string* v = SectionFileIO::FindEntry(section, "ParentClass")) outData.parentClass = *v;
        if (const std::string* v = SectionFileIO::FindEntry(section, "UIScale")) outData.uiScale = SafeParse::ToFloat(*v, 1.0f);
        if (const std::string* v = SectionFileIO::FindEntry(section, "ForceNativeResolution")) outData.forceNativeResolution = (*v == "true");
        if (const std::string* v = SectionFileIO::FindEntry(section, "ProgrammingInterface")) outData.programmingInterface = *v;
    }

    outData.rootElement = WidgetElementIO::ReadElementTree(sections);

    if (!outData.programmingInterface.empty()) {
        const ProgrammingInterfaceInfo* provider = ProgrammingInterfaceRegistry::Get().Find(outData.programmingInterface);
        if (provider && provider->loadContent) outData.behaviorContent = provider->loadContent(sections, "Widget.");
    }

    return true;
}

Widget* InstantiateWidget(const std::string& path) {
    const WidgetAssetData* cached = WidgetAssetCache::Get().GetOrLoad(path);
    if (!cached) return nullptr;

    Widget* widget = new Widget();
    widget->SetRoot(WidgetElementIO::CloneElementTree(cached->rootElement));
    widget->uiScale = cached->uiScale;
    widget->forceNativeResolution = cached->forceNativeResolution;

    if (!cached->programmingInterface.empty() && cached->behaviorContent) {
        const ProgrammingInterfaceInfo* provider = ProgrammingInterfaceRegistry::Get().Find(cached->programmingInterface);
        if (provider && provider->createWidgetBehavior) {
            widget->SetBehavior(provider->createWidgetBehavior(*cached->behaviorContent));
        }
    }

    return widget;
}

}
