#include "WidgetAsset.h"
#include "Core/Widgets/WidgetElementIO.h"
#include "Core/SafeParse.h"
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
    out << "\n";

    WidgetElementIO::WriteElementTree(out, data.rootElement);

    data.graph.SaveGraphContent(out, "Widget.");

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
    }

    outData.rootElement = WidgetElementIO::ReadElementTree(sections);
    outData.graph.LoadGraphContent(sections, "Widget.");

    return true;
}

}
