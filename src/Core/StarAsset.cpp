#include "StarAsset.h"
#include <fstream>

bool StarAsset::SaveToFile(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "[StarAsset]\n";
    out << "Type=" << assetType << "\n";
    out << "Source=" << sourcePath << "\n";

    return true;
}

bool StarAsset::LoadFromFile(const std::string& path, StarAsset& outAsset) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '[') continue;

        size_t sep = line.find('=');
        if (sep == std::string::npos) continue;

        std::string key = line.substr(0, sep);
        std::string value = line.substr(sep + 1);

        if (key == "Type") outAsset.assetType = value;
        else if (key == "Source") outAsset.sourcePath = value;
    }

    return true;
}
