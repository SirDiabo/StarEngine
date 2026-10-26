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
    bool requiresRestart = false;

    // Other module names this module requires to function.
    std::string repository;

    std::vector<std::string> dependencies;
    // Extra asset extensions this module registers, packaged as text assets when cooking.
    std::vector<std::string> assetTypes;

    // Directory containing the manifest file and the module's source.
    std::string sourcePath;

    bool hasExternalDependencies = false;

    static bool LoadFromFile(const std::string& filePath, ModuleManifest& outManifest);
    static bool SaveToFile(const std::string& filePath, const ModuleManifest& manifest);
};
