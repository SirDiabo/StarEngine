#pragma once
#include <string>
#include <unordered_map>

namespace SaveFileFormat {
    std::unordered_map<std::string, std::string> ReadEntries(const std::string& saveFileName);

    bool WriteEntries(const std::string& saveFileName, const std::unordered_map<std::string, std::string>& updates);
}
