#include "SectionFileIO.h"

namespace SectionFileIO {

std::vector<ParsedSection> ParseSections(std::ifstream& in) {
    std::vector<ParsedSection> sections;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            ParsedSection section;
            section.name = line.substr(1, line.size() - 2);
            sections.push_back(section);
            continue;
        }

        if (sections.empty()) continue;

        size_t separator = line.find('=');
        if (separator == std::string::npos) continue;

        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        sections.back().entries.emplace_back(key, value);
    }
    return sections;
}

const std::string* FindEntry(const ParsedSection& section, const std::string& key) {
    for (const auto& entry : section.entries) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

}
