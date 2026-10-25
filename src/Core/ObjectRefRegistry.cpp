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

std::string ObjectRefRegistry::Encode(const std::string& kind, unsigned int objectId) {
    return kind + ":" + std::to_string(objectId);
}

bool ObjectRefRegistry::TryDecode(const std::string& encoded, std::string& outKind, unsigned int& outId) {
    size_t sep = encoded.find(':');
    if (sep == std::string::npos) return false;
    try {
        outId = (unsigned int)std::stoul(encoded.substr(sep + 1));
    } catch (...) {
        return false;
    }
    outKind = encoded.substr(0, sep);
    return true;
}
