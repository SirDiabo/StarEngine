#pragma once
#include "Core/Panel.h"
#include "Core/Module.h"
#include "Core/ModuleImporter.h"
#include "Core/AsyncTask.h"
#include "ChoicePopup.h"
#include <string>
#include <vector>
#include <unordered_map>

class LogPanel;

class ModuleBrowserPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

    void SetLogPanel(LogPanel* panel) { logPanel = panel; }

private:
    enum class InputMode { None, RepositoryEntry, CatalogList };

    void RenderRow(const ModuleEntry& entry, float y, float rowHeight);
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
    void ApplyPendingToggles();

    LogPanel* logPanel = nullptr;
    std::string statusMessage;
    float scrollOffset = 0.0f;

    ChoicePopup importMenu;
    InputMode inputMode = InputMode::None;
    char repoInputBuffer[256] = {0};
    bool repoInputEditing = false;

    std::vector<CatalogEntry> catalogEntries;
    std::string catalogError;
    bool catalogLoading = false;

    ChoicePopup engineOrProjectPopup;
    std::string pendingRelocateDir;

    std::unordered_map<std::string, bool> pendingToggles;

    AsyncTask rebuildTask;
    bool rebuildRunning = false;
};
