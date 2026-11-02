#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

enum class ImportMessageLevel { Info, Warning, Error };

struct ImportMessage {
    ImportMessageLevel level = ImportMessageLevel::Info;
    std::string text;
};

enum class ImportOptionKind { Bool, Enum, Int, Float };

struct ImportOptionDef {
    std::string id;
    std::string label;
    std::string group;
    ImportOptionKind kind = ImportOptionKind::Bool;
    std::vector<std::string> choices;
    std::string defaultValue;
    float minValue = 0.0f;
    float maxValue = 1000000.0f;
    std::string showIfId;
    std::string showIfValue;
};

struct ImportContext {
    std::string sourcePath;
    std::string targetFolder;
    std::string baseName;
    std::map<std::string, std::string> options;

    bool GetBool(const std::string& id, bool fallback = false) const;
    int GetInt(const std::string& id, int fallback = 0) const;
    float GetFloat(const std::string& id, float fallback = 0.0f) const;
    std::string GetString(const std::string& id, const std::string& fallback = std::string()) const;
};

struct ImportResult {
    bool success = false;
    bool showReport = false;
    std::string summary;
    std::vector<std::string> createdAssets;
    std::vector<ImportMessage> messages;

    void Info(const std::string& text) { messages.push_back({ ImportMessageLevel::Info, text }); }
    void Warn(const std::string& text) { messages.push_back({ ImportMessageLevel::Warning, text }); }
    void Error(const std::string& text) { messages.push_back({ ImportMessageLevel::Error, text }); }
    bool HasProblems() const;
};

struct ImportHandler {
    std::string extension;
    std::string moduleName;
    std::string label;
    std::vector<ImportOptionDef> options;
    std::function<ImportResult(const ImportContext& context)> import;
};

class ImportHandlerRegistry {
public:
    static ImportHandlerRegistry& Get();

    void Register(const ImportHandler& handler);
    const ImportHandler* Find(const std::string& extension) const;
    const std::vector<ImportHandler>& GetAll() const { return handlers; }

    ImportResult Import(const std::string& sourcePath, const std::string& targetFolder,
                        const std::map<std::string, std::string>& options = {}) const;

    static std::map<std::string, std::string> DefaultOptions(const ImportHandler& handler);

    static std::string UniquePath(const std::string& folder, const std::string& baseName, const std::string& extension);

private:
    std::vector<ImportHandler> handlers;
};
