#include "LayoutSerializer.h"
#include <fstream>
#include <sstream>

bool LayoutSerializer::Save(const std::string& filePath,
                             const std::unordered_map<std::string, std::string>& data) {
    std::ofstream f(filePath);
    if (!f.is_open()) return false;
    f << "# StarEngine key-value config\n";
    for (const auto& kv : data) {
        f << kv.first << "=" << kv.second << "\n";
    }
    return true;
}

bool LayoutSerializer::Load(const std::string& filePath,
                              std::unordered_map<std::string, std::string>& data) {
    std::ifstream f(filePath);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key   = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        data[key] = value;
    }
    return true;
}