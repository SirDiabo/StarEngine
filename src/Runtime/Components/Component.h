#pragma once
#include "Runtime/Object.h"
#include "Runtime/Property.h"
#include "raylib.h"
#include <string>
#include <unordered_map>

class PropertyList;
class Actor;

class Component : public Object {
public:
    Vector3 position = {0, 0, 0};
    Vector3 rotation = {0, 0, 0};
    Vector3 scale = {1, 1, 1};

    Component* parent = nullptr;
    Actor* owningActor = nullptr;
    Actor* GetOwningActor() const { return owningActor; }

    std::unordered_map<std::string, std::string> savableFields;

    virtual void GetEditorProperties(PropertyList& outProperties) { }
    virtual void Update(float deltaTime) = 0;

    virtual void GetProperties(PropertyList& outProperties) override {
        Object::GetProperties(outProperties);
        outProperties.AddVector3("Position", &position.x);
        outProperties.AddVector3("Rotation", &rotation.x);
        outProperties.AddVector3("Scale", &scale.x);
        GetEditorProperties(outProperties);
    }
};