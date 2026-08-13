#pragma once
#include <string>
#include <vector>

struct ModuleManifest {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string category;
    bool enabledByDefault = true;
    bool engineModule = false;
    bool editorOnly = false;

    // Other module names this module requires to function.
    std::string repository;

    std::vector<std::string> dependencies;
    // Asset extensions this module registers (e.g. .stargraph).
    std::vector<std::string> assetTypes;
    // Stargraph node categories this module contributes (e.g. Audio, Physics).
    std::vector<std::string> nodeCategories;

    // Directory containing the manifest file and the module's source.
    std::string sourcePath;

    bool hasExternalDependencies = false;

    static bool LoadFromFile(const std::string& filePath, ModuleManifest& outManifest);
    static bool SaveToFile(const std::string& filePath, const ModuleManifest& manifest);
};
