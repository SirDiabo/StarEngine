#pragma once
#include <unordered_map>

class Object;

class ObjectRefRegistry {
public:
    static ObjectRefRegistry& Get();

    void Register(Object* obj);
    void Unregister(Object* obj);
    Object* Resolve(unsigned int id) const;

private:
    std::unordered_map<unsigned int, Object*> objects;
};
