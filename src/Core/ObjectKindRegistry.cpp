#include "ObjectKindRegistry.h"

ObjectKindRegistry& ObjectKindRegistry::Get() {
    static ObjectKindRegistry instance;
    return instance;
}

void ObjectKindRegistry::RegisterKind(const std::string& kind, const std::string& parentKind) {
    parents[kind] = parentKind;
}

std::string ObjectKindRegistry::GetParentKind(const std::string& kind) const {
    auto it = parents.find(kind);
    return it != parents.end() ? it->second : std::string();
}

bool ObjectKindRegistry::IsCompatible(const std::string& candidateKind, const std::string& requiredKind) const {
    if (candidateKind.empty() || requiredKind.empty() || candidateKind == requiredKind) return true;

    std::string current = candidateKind;
    for (int guard = 0; guard < 32; guard++) {
        auto it = parents.find(current);
        if (it == parents.end() || it->second.empty()) return false;
        current = it->second;
        if (current == requiredKind) return true;
    }
    return false;
}
