#pragma once
#include <string>
#include <unordered_map>
#include <vector>

struct ComponentDecl {
    std::string componentType = "SceneComponent";
    std::string name;

    // Name of another component on the same actor this one is parented to.
    std::string parentName;

    // Transform relative to the parent named above (or the actor's root component if empty).
    float posX = 0, posY = 0, posZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0;
    float scaleX = 1, scaleY = 1, scaleZ = 1;

    std::unordered_map<std::string, std::string> fields;
    std::unordered_map<std::string, std::vector<std::string>> arrayFields;

    std::string Get(const std::string& key, const std::string& def = "") const {
        auto it = fields.find(key);
        return it != fields.end() ? it->second : def;
    }

    // SaveFileRegistry
    std::unordered_map<std::string, std::string> savableFields;
};
