#pragma once
#include <string>
#include <unordered_map>

class SaveFileRegistry {
public:
    static const char* Category() { return "SaveFiles"; }

    static SaveFileRegistry& Get();

    void Load(const std::string& projectRoot);
    void Save() const;

    bool IsEncrypted(const std::string& name) const;
    void SetEncrypted(const std::string& name, bool encrypted);

private:
    std::string filePath;
    std::unordered_map<std::string, bool> encryptedFlags;
};
