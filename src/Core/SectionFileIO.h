#pragma once
#include <string>
#include <vector>
#include <utility>
#include <fstream>

namespace SectionFileIO {
    struct ParsedSection {
        std::string name;
        std::vector<std::pair<std::string, std::string>> entries;
    };

    std::vector<ParsedSection> ParseSections(std::ifstream& in);
    const std::string* FindEntry(const ParsedSection& section, const std::string& key);
}
