#pragma once
#include "ComponentRegistry.h"
#include <string>
#include <unordered_map>
#include <vector>

class LiveComponentCache {
public:
    LiveComponentCache() = default;
    LiveComponentCache(const LiveComponentCache&) = delete;
    LiveComponentCache& operator=(const LiveComponentCache&) = delete;
    ~LiveComponentCache();

    void Clear();
    Component* Acquire(const std::string& key, const ComponentDecl& decl);
    void MarkInSync(const std::string& key, const ComponentDecl& decl);

private:
    struct Entry {
        Component* component = nullptr;
        std::string componentType;
        std::string signature;
    };
    std::unordered_map<std::string, Entry> entries;
};

namespace ComponentEditing {

std::string DeclSignature(const ComponentDecl& decl);
std::string ReadField(LiveComponentCache& cache, const std::string& key, const ComponentDecl& decl,
                      const ComponentFieldDesc& field);
void WriteField(LiveComponentCache& cache, const std::string& key, ComponentDecl& decl,
                const std::string& fieldKey, const std::string& value);
void WriteArray(LiveComponentCache& cache, const std::string& key, ComponentDecl& decl,
                const std::string& arrayKey, const std::vector<std::string>& values);

}
