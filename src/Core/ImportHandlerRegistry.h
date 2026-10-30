#pragma once
#include <functional>
#include <string>
#include <vector>

enum class ImportMessageLevel { Info, Warning, Error };

struct ImportMessage {
    ImportMessageLevel level = ImportMessageLevel::Info;
    std::string text;
};

struct ImportContext {
    std::string sourcePath;
    std::string targetFolder;
    std::string baseName;
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
    std::function<ImportResult(const ImportContext& context)> import;
};

class ImportHandlerRegistry {
public:
    static ImportHandlerRegistry& Get();

    void Register(const ImportHandler& handler);
    const ImportHandler* Find(const std::string& extension) const;
    const std::vector<ImportHandler>& GetAll() const { return handlers; }

    ImportResult Import(const std::string& sourcePath, const std::string& targetFolder) const;

    static std::string UniquePath(const std::string& folder, const std::string& baseName, const std::string& extension);

private:
    std::vector<ImportHandler> handlers;
};
