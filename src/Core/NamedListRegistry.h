#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class NamedListRegistry {
public:
    static NamedListRegistry& Get();

    void Load(const std::string& projectRoot);
    void Save() const;

    const std::vector<std::string>& GetList(const std::string& category) const;
    bool AddEntry(const std::string& category, const std::string& name);
    bool RemoveEntry(const std::string& category, const std::string& name);
    bool HasEntry(const std::string& category, const std::string& name) const;
    bool HasCategory(const std::string& category) const;
    void SetList(const std::string& category, const std::vector<std::string>& names);

private:
    std::string filePath;
    std::unordered_map<std::string, std::vector<std::string>> lists;
};
