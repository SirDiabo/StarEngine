#pragma once
#include "Core/SectionFileIO.h"
#include <ostream>
#include <string>
#include <vector>

class WidgetElement;

namespace WidgetElementIO {
    void WriteElementTree(std::ostream& out, WidgetElement* root);
    WidgetElement* ReadElementTree(const std::vector<SectionFileIO::ParsedSection>& sections);
    WidgetElement* CloneElementTree(WidgetElement* root);
}
