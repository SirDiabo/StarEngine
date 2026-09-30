#pragma once
#include <string>
#include <vector>
#include <unordered_map>

enum class InputMappingType { Bool, Axis, Axis2D };

struct InputMappingBinding {
    std::string key;
    float scale = 1.0f;
    int axis = 0; // Axis2D only: 0 = X, 1 = Y
    bool requireCtrl = false;
    bool requireAlt = false;
    bool requireShift = false;
};

struct InputMapping {
    InputMappingType type = InputMappingType::Bool;
    std::vector<InputMappingBinding> bindings;
};

class InputMappingRegistry {
public:
    static const char* Category() { return "InputMappings"; }

    static InputMappingRegistry& Get();

    void Load(const std::string& projectRoot);
    void LoadFromText(const std::string& text);
    void Save() const;

    const InputMapping* Find(const std::string& name) const;
    void SetMapping(const std::string& name, const InputMapping& mapping);
    void Remove(const std::string& name);

    void SetMappingRuntime(const std::string& name, const InputMapping& mapping);

private:
    std::string filePath;
    std::unordered_map<std::string, InputMapping> mappings;
};
