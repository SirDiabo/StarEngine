#pragma once
#include <string>
#include <vector>

enum class PropertyType {
    Float,
    Int,
    Bool,
    String,
    Vector3
};

struct PropertyHandle {
    std::string name;
    PropertyType type;
    void* pointer = nullptr;
    bool readOnly = false;

    float& AsFloat() const { return *static_cast<float*>(pointer); }
    int& AsInt() const { return *static_cast<int*>(pointer); }
    bool& AsBool() const { return *static_cast<bool*>(pointer); }
    std::string& AsString() const { return *static_cast<std::string*>(pointer); }
    float* AsVector3() const { return static_cast<float*>(pointer); }
};

class PropertyList {
public:
    void AddFloat(const std::string& name, float* value, bool readOnly = false) {
        PropertyHandle handle;
        handle.name = name;
        handle.type = PropertyType::Float;
        handle.pointer = value;
        handle.readOnly = readOnly;
        properties.push_back(handle);
    }

    void AddInt(const std::string& name, int* value, bool readOnly = false) {
        PropertyHandle handle;
        handle.name = name;
        handle.type = PropertyType::Int;
        handle.pointer = value;
        handle.readOnly = readOnly;
        properties.push_back(handle);
    }

    void AddBool(const std::string& name, bool* value, bool readOnly = false) {
        PropertyHandle handle;
        handle.name = name;
        handle.type = PropertyType::Bool;
        handle.pointer = value;
        handle.readOnly = readOnly;
        properties.push_back(handle);
    }

    void AddString(const std::string& name, std::string* value, bool readOnly = false) {
        PropertyHandle handle;
        handle.name = name;
        handle.type = PropertyType::String;
        handle.pointer = value;
        handle.readOnly = readOnly;
        properties.push_back(handle);
    }

    void AddVector3(const std::string& name, float* xyz, bool readOnly = false) {
        PropertyHandle handle;
        handle.name = name;
        handle.type = PropertyType::Vector3;
        handle.pointer = xyz;
        handle.readOnly = readOnly;
        properties.push_back(handle);
    }

    const std::vector<PropertyHandle>& GetAll() const { return properties; }

private:
    std::vector<PropertyHandle> properties;
};
