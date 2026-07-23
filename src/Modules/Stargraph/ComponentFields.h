#pragma once
#include "Stargraph.h"
#include <functional>
#include <vector>

class Component;

enum class ComponentFieldType { String, Float, Bool, StringArray };

struct ComponentFieldDesc {
    const char* componentType = nullptr;
    const char* key = nullptr;
    ComponentFieldType type = ComponentFieldType::String;
    std::string ComponentDecl::* stringField = nullptr;
    float ComponentDecl::* floatField = nullptr;
    bool ComponentDecl::* boolField = nullptr;
    std::vector<std::string> ComponentDecl::* stringArrayField = nullptr;

    std::function<void(Component&, const ComponentDecl&)> applyToComponent;
};

// Add fields here.
const std::vector<ComponentFieldDesc>& GetComponentFields();

using ComponentFactory = std::function<Component*()>;

struct ComponentTypeDesc {
    const char* componentType = nullptr;
    ComponentFactory factory;
};

const std::vector<ComponentTypeDesc>& GetComponentTypes();