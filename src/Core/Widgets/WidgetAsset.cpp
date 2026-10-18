#include "WidgetAsset.h"
#include "WidgetAssetCache.h"
#include "Widget.h"
#include "WidgetElementIO.h"
#include "Core/SafeParse.h"
#include "Core/ContentPathResolver.h"
#include "Core/CookedFile.h"
#include "Core/ProgrammingInterfaceRegistry.h"
#include "Runtime/WidgetBehavior.h"
#include "raylib.h"
#include <fstream>
#include <sstream>

namespace WidgetAsset {

bool SaveWidgetAsset(const std::string& path, const WidgetAssetData& data) {
    std::string resolvedPath = ContentPathResolver::Resolve(path);
    std::ofstream out(resolvedPath);
    if (!out.is_open()) {
        TraceLog(LOG_WARNING, "WidgetAsset: failed to open \"%s\" for writing", resolvedPath.c_str());
        return false;
    }
    return WriteWidgetAsset(out, data);
}

std::string SerializeWidgetAsset(const WidgetAssetData& data, bool includeBehavior) {
    std::ostringstream out;
    WriteWidgetAsset(out, data, includeBehavior);
    return out.str();
}

bool WriteWidgetAsset(std::ostream& out, const WidgetAssetData& data, bool includeBehavior) {
    out << "[Widget]\n";
    out << "Version=1\n";
    out << "Name=" << data.assetName << "\n";
    out << "ParentClass=" << data.parentClass << "\n";
    if (data.uiScale != 1.0f) out << "UIScale=" << data.uiScale << "\n";
    if (data.forceNativeResolution) out << "ForceNativeResolution=true\n";
    if (!data.programmingInterface.empty()) out << "ProgrammingInterface=" << data.programmingInterface << "\n";
    out << "\n";

    WidgetElementIO::WriteElementTree(out, data.rootElement);

    if (includeBehavior && !data.programmingInterface.empty() && data.behaviorContent) {
        const ProgrammingInterfaceInfo* provider = ProgrammingInterfaceRegistry::Get().Find(data.programmingInterface);
        if (provider && provider->saveContent) provider->saveContent(out, *data.behaviorContent, "Widget.");
    }

    return true;
}

bool LoadWidgetAsset(const std::string& path, WidgetAssetData& outData) {
    std::string openPath = CookedFile::Open(path);
    std::ifstream in(openPath);
    if (!in.is_open()) {
        TraceLog(LOG_WARNING, "WidgetAsset: failed to open \"%s\" for reading", openPath.c_str());
        return false;
    }
    return ReadWidgetAsset(in, outData);
}

bool DeserializeWidgetAsset(const std::string& text, WidgetAssetData& outData, bool loadBehavior) {
    std::istringstream in(text);
    return ReadWidgetAsset(in, outData, loadBehavior);
}

bool ReadWidgetAsset(std::istream& in, WidgetAssetData& outData, bool loadBehavior) {
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

    if (loadBehavior && !outData.programmingInterface.empty()) {
        const ProgrammingInterfaceInfo* provider = ProgrammingInterfaceRegistry::Get().Find(outData.programmingInterface);
        if (provider && provider->loadContent) outData.behaviorContent = provider->loadContent(sections, "Widget.");
    }

    return true;
}

void AttachWidgetBehavior(Widget* widget, const std::string& path) {
    if (!widget) return;
    const WidgetAssetData* cached = WidgetAssetCache::Get().GetOrLoad(path);
    if (!cached || cached->programmingInterface.empty() || !cached->behaviorContent) return;
    const ProgrammingInterfaceInfo* provider = ProgrammingInterfaceRegistry::Get().Find(cached->programmingInterface);
    if (provider && provider->createWidgetBehavior) {
        widget->SetBehavior(provider->createWidgetBehavior(*cached->behaviorContent));
    }
}

Widget* InstantiateWidget(const std::string& path, bool withBehavior) {
    const WidgetAssetData* cached = WidgetAssetCache::Get().GetOrLoad(path);
    if (!cached) return nullptr;
    Widget* widget = new Widget();
    widget->SetRoot(WidgetElementIO::CloneElementTree(cached->rootElement));
    widget->uiScale = cached->uiScale;
    widget->forceNativeResolution = cached->forceNativeResolution;
    if (withBehavior) AttachWidgetBehavior(widget, path);
    return widget;
}

}
