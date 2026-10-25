#pragma once
#include "ComponentDecl.h"
#include <istream>
#include <ostream>
#include <string>
#include <vector>

struct ActorInstance {
    std::string name;
    std::string className;
    std::string classAssetPath;
    std::string meshAssetPath;
    std::vector<std::string> materialAssetPaths;
    bool castsShadow = true;

    bool hasComponent = false;
    ComponentDecl component;
    std::string actorKind;

    float posX = 0, posY = 0, posZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0;
    float scaleX = 1, scaleY = 1, scaleZ = 1;
};

class StarMap {
public:
    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path);
    bool SaveToStream(std::ostream& out) const;
    bool LoadFromStream(std::istream& in);
    std::string SerializeToString() const;
    bool LoadFromString(const std::string& text);

    void AddInstance(const ActorInstance& instance);
    void RemoveInstance(size_t index);

    const std::vector<ActorInstance>& GetInstances() const { return instances; }
    std::vector<ActorInstance>& GetInstances() { return instances; }

    std::string mapName;
    int formatVersion = 1;

private:
    std::vector<ActorInstance> instances;
};
