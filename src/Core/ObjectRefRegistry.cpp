#include "ObjectRefRegistry.h"
#include "Runtime/Object.h"

ObjectRefRegistry& ObjectRefRegistry::Get() {
    static ObjectRefRegistry instance;
    return instance;
}

void ObjectRefRegistry::Register(Object* obj) {
    if (obj) objects[obj->GetObjectId()] = obj;
}

void ObjectRefRegistry::Unregister(Object* obj) {
    if (!obj) return;
    auto it = objects.find(obj->GetObjectId());
    if (it != objects.end() && it->second == obj) objects.erase(it);
}

Object* ObjectRefRegistry::Resolve(unsigned int id) const {
    auto it = objects.find(id);
    return it != objects.end() ? it->second : nullptr;
}
