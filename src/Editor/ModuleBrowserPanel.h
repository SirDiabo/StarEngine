#pragma once
#include "Core/Panel.h"
#include "Core/Module.h"
#include "Core/ModuleImporter.h"
#include "Core/AsyncTask.h"
#include "ChoicePopup.h"
#include "TrustProjectPopup.h"
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

class Widget;
class TextElement;

class LogPanel;

class ModuleBrowserPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    float CacheRefreshInterval() const override { return rebuildRunning ? -1.0f : 0.1f; }

    void SetLogPanel(LogPanel* panel) { logPanel = panel; }

private:
    enum class InputMode { None, RepositoryEntry, CatalogList };

    void RenderImportBar();
    void RenderRepositoryEntry(Rectangle area);
    void RenderCatalogList(Rectangle area);

    void ImportFromDisk();
    void BeginRepositoryEntry();
    void ConfirmRepositoryEntry(const std::string& ownerRepo);
    void BeginCatalogBrowse();
    void HandleImportedModule(const std::string& destDir);
    void FinishImport(bool success, const std::string& message);
    void StartRebuildAsync();
    void RunWhenTrusted(std::function<void()> action);
    void ApplyPendingToggles();

    LogPanel* logPanel = nullptr;
    std::string statusMessage;

    ChoicePopup importMenu;
    InputMode inputMode = InputMode::None;

    std::vector<CatalogEntry> catalogEntries;
    std::string catalogError;
    bool catalogLoading = false;

    ChoicePopup engineOrProjectPopup;
    TrustProjectPopup trustPopup;
    std::function<void()> actionAfterTrust;
    std::string pendingRelocateDir;

    std::unordered_map<std::string, bool> pendingToggles;

    AsyncTask rebuildTask;
    bool rebuildRunning = false;

    Widget* listChrome = nullptr;
    size_t builtListCount = (size_t)-1;

    Widget* controlsChrome = nullptr;

    Widget* repoChrome = nullptr;
    TextElement* repoTextField = nullptr;

    void EnsureListChrome();
    void EnsureControlsChrome();
    void EnsureRepoChrome();
};
