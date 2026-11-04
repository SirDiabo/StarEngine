#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class ObjectKindRegistry {
public:
    static ObjectKindRegistry& Get();

    void RegisterKind(const std::string& kind, const std::string& parentKind);
    std::vector<std::string> GetAllKinds() const;
    void RegisterInterface(const std::string& kind, const std::string& interfaceKind);
    bool IsCompatible(const std::string& candidateKind, const std::string& requiredKind) const;
    std::string GetParentKind(const std::string& kind) const;

private:
    ObjectKindRegistry();
    void RegisterBuiltInKinds();

    std::unordered_map<std::string, std::string> parents;
    std::unordered_multimap<std::string, std::string> interfaces;
};
