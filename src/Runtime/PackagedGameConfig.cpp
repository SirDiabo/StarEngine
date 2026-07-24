#include "PackagedGameConfig.h"
#include "Core/SafeParse.h"
#include <fstream>
#include <sstream>

bool PackagedGameConfig::LoadFromText(const std::string& text, PackagedGameConfig& out) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t sep = line.find('=');
        if (sep == std::string::npos) continue;

        std::string key = line.substr(0, sep);
        std::string value = line.substr(sep + 1);

        if (key == "name") out.name = value;
        else if (key == "renderScale") out.renderScale = SafeParse::ToFloat(value);
        else if (key == "defaultMap") out.defaultMap = value;
        else if (key == "renderer") out.renderer = value;
        else if (key == "cipher") out.cipher = value;
        else if (key == "cipherKey") out.cipherKey = value;
    }
    return true;
}

bool PackagedGameConfig::LoadFromFile(const std::string& path, PackagedGameConfig& out) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::ostringstream text;
    text << in.rdbuf();
    return LoadFromText(text.str(), out);
}

std::string PackagedGameConfig::ToText() const {
    std::ostringstream out;
    out << "name=" << name << "\n";
    out << "renderScale=" << renderScale << "\n";
    out << "defaultMap=" << defaultMap << "\n";
    out << "renderer=" << renderer << "\n";
    out << "cipher=" << cipher << "\n";
    out << "cipherKey=" << cipherKey << "\n";
    return out.str();
}