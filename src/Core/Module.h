#pragma once
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

protected:
    std::string name;
    bool enabled = true;
};

class ModuleManager {
public:
    static ModuleManager& Get();

    void Register(Module* module);
    bool InitializeAll();
    void ShutdownAll();
    void UpdateAll(float deltaTime);

private:
    std::vector<Module*> modules;
};