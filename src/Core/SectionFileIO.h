#pragma once
#include <string>
#include <vector>
#include <utility>
#include <istream>

namespace SectionFileIO {
    struct ParsedSection {
        std::string name;
        std::vector<std::pair<std::string, std::string>> entries;
    };

    std::vector<ParsedSection> ParseSections(std::istream& in);
    const std::string* FindEntry(const ParsedSection& section, const std::string& key);
}
