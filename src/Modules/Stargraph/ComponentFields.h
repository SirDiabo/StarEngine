#pragma once
#include "Stargraph.h"
#include <vector>

enum class ComponentFieldType { String, Float, Bool, StringArray };

struct ComponentFieldDesc {
    const char* componentType = nullptr;
    const char* key = nullptr;
    ComponentFieldType type = ComponentFieldType::String;
    std::string ComponentDecl::* stringField = nullptr;
    float ComponentDecl::* floatField = nullptr;
    bool ComponentDecl::* boolField = nullptr;
    std::vector<std::string> ComponentDecl::* stringArrayField = nullptr;
};

// Add fields here.
const std::vector<ComponentFieldDesc>& GetComponentFields();