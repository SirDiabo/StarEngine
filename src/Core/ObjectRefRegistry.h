#pragma once
#include <string>
#include <unordered_map>

class Object;

class ObjectRefRegistry {
public:
    static ObjectRefRegistry& Get();

    void Register(Object* obj);
    void Unregister(Object* obj);
    Object* Resolve(unsigned int id) const;

    static std::string Encode(const std::string& kind, unsigned int objectId);
    static bool TryDecode(const std::string& encoded, std::string& outKind, unsigned int& outId);

private:
    std::unordered_map<unsigned int, Object*> objects;
};
