#pragma once
#include "Property.h"
#include <atomic>
#include <string>

class Object {
public:
    virtual ~Object() = default;

    virtual const char* GetClassName() const { return "Object"; }
    virtual void GetProperties(PropertyList& outProperties) {
        outProperties.AddString("Name", &name);
    }

    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName) { name = newName; }

    unsigned int GetObjectId() const { return objectId; }

protected:
    std::string name = "Object";

private:
    unsigned int objectId = GenerateObjectId();
    static unsigned int GenerateObjectId() {
        static std::atomic<unsigned int> nextId{1};
        return nextId.fetch_add(1);
    }
};
