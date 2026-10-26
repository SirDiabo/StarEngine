#include "ModuleManifest.h"
#include "LayoutSerializer.h"
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

static std::vector<std::string> SplitCsv(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) result.push_back(item);
    }
    return result;
}

static std::string JoinCsv(const std::vector<std::string>& values) {
    std::string result;
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) result += ",";
        result += values[i];
    }
    return result;
}

bool ModuleManifest::LoadFromFile(const std::string& filePath, ModuleManifest& outManifest) {
    std::unordered_map<std::string, std::string> data;
    if (!LayoutSerializer::Load(filePath, data)) return false;
    if (data.find("name") == data.end()) return false;

    outManifest = ModuleManifest();
    outManifest.name        = data["name"];
    outManifest.version     = data.count("version")     ? data["version"]     : "1.0";
    outManifest.description = data.count("description") ? data["description"] : "";
    outManifest.author      = data.count("author")      ? data["author"]      : "";
    outManifest.category    = data.count("category")    ? data["category"]    : "Other";
    outManifest.repository  = data.count("repository")  ? data["repository"]  : "";
    outManifest.enabledByDefault = !data.count("enabledByDefault") || data["enabledByDefault"] != "false";
    outManifest.engineModule = data.count("engineModule") && data["engineModule"] == "true";
    outManifest.editorOnly = data.count("editorOnly") && data["editorOnly"] == "true";
    outManifest.requiresRestart = data.count("requiresRestart") && data["requiresRestart"] == "true";

    if (data.count("dependencies"))   outManifest.dependencies   = SplitCsv(data["dependencies"]);
    if (data.count("assetTypes"))     outManifest.assetTypes     = SplitCsv(data["assetTypes"]);

    outManifest.sourcePath = fs::path(filePath).parent_path().string();
    outManifest.hasExternalDependencies = fs::exists(fs::path(outManifest.sourcePath) / "CMakeLists.txt");
    return true;
}

bool ModuleManifest::SaveToFile(const std::string& filePath, const ModuleManifest& manifest) {
    std::unordered_map<std::string, std::string> data;
    data["name"]              = manifest.name;
    data["version"]           = manifest.version;
    data["description"]       = manifest.description;
    data["author"]            = manifest.author;
    data["category"]          = manifest.category;
    data["repository"]        = manifest.repository;
    data["enabledByDefault"]  = manifest.enabledByDefault ? "true" : "false";
    data["engineModule"]      = manifest.engineModule ? "true" : "false";
    data["editorOnly"]        = manifest.editorOnly ? "true" : "false";
    data["requiresRestart"]   = manifest.requiresRestart ? "true" : "false";
    data["dependencies"]      = JoinCsv(manifest.dependencies);
    data["assetTypes"]        = JoinCsv(manifest.assetTypes);
    return LayoutSerializer::Save(filePath, data);
}
