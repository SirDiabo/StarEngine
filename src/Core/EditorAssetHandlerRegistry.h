#pragma once
#include <string>
#include <vector>
#include <functional>

class EditorTab;
class LogPanel;
class ViewportPanel;

struct EditorServices {
    LogPanel* sharedLogPanel = nullptr;
    ViewportPanel* sharedViewportPanel = nullptr;
};

struct EditorAssetHandlerInfo {
    std::string extension;
    std::string moduleName;
    std::function<EditorTab*(const std::string& fullPath, const EditorServices& services)> createTab;
};

class EditorAssetHandlerRegistry {
public:
    static EditorAssetHandlerRegistry& Get();

    void RegisterHandler(const EditorAssetHandlerInfo& info);
    const EditorAssetHandlerInfo* FindHandler(const std::string& extension) const;

private:
    std::vector<EditorAssetHandlerInfo> handlers;
};
