#pragma once
#include "ModuleManifest.h"
#include <string>
#include <vector>

class Module {
public:
    virtual ~Module() = default;
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(float deltaTime) = 0;

    const std::string& GetName() const { return name; }
    bool IsEnabled() const { return enabled; }
    void SetEnabled(bool value) { enabled = value; }

protected:
    std::string name;
    bool enabled = true;
};

struct ModuleEntry {
    Module* instance = nullptr;
    ModuleManifest manifest;
    bool hasManifest = false;
};

class ModuleManager {
public:
    static ModuleManager& Get();

    void Register(Module* module);
    bool InitializeAll();
    void ShutdownAll();
    void UpdateAll(float deltaTime);

    void ScanManifests();

    const std::vector<ModuleEntry>& GetEntries() const { return entries; }
    ModuleEntry* FindEntry(const std::string& moduleName);

    void SetModuleEnabled(const std::string& moduleName, bool value);
    bool IsModuleEnabled(const std::string& moduleName) const;

    std::vector<std::string> GetDisabledModuleNames() const;
    void ApplyDisabledModuleNames(const std::vector<std::string>& disabledNames);

    static std::string GetModulesSourceRoot();

    static std::string GetEngineModulesRoot();

    static std::string GetProjectModulesRoot();

private:
    std::vector<Module*> modules;
    std::vector<ModuleEntry> entries;

    void RebuildEntries();
};