#include "ModuleBrowserPanel.h"
#include "NativeFileDialog.h"
#include "Core/Module.h"
#include "Core/ModuleManifest.h"
#include "Core/BuildRunner.h"
#include "Core/Project.h"
#include "Core/ModuleImporter.h"
#include "LogPanel.h"
#include "LoadingPanel.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/BoxElement.h"
#include "Core/Widgets/VerticalBoxElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/CheckBoxElement.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace {

static std::string EntryDisplayName(const ModuleEntry& entry) {
    return entry.instance ? entry.instance->GetName() : entry.manifest.name;
}

class ModuleRowElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "ModuleRowElement"; }
    std::string GetObjectKind() const override { return "ModuleRowElement"; }

    std::string title, versionText, tagText, restartText, descriptionText, depsText;
    Color tagColor{};
    bool hasCheckbox = true;

protected:
    Rectangle ComputeChildRect(WidgetElement*, Rectangle ownRect, float, int) const override {
        return { ownRect.x + 8, ownRect.y + 8, 18, 18 };
    }

    void OnDrawSelf(Rectangle rect, float) override {
        const UIPalette& theme = UITheme::Get();
        DrawRectangleRec(rect, theme.buttonBg);
        DrawRectangleLinesEx(rect, 1, theme.buttonBorder);

        if (!hasCheckbox) {
            Rectangle checkRect = { rect.x + 8, rect.y + 8, 18, 18 };
            DrawRectangleLinesEx(checkRect, 1, theme.buttonBorder);
        }

        int nextLabelX = (int)rect.x + 8 + 26;
        Color titleColor = theme.textPrimary;
        DrawText(title.c_str(), nextLabelX, (int)rect.y + 6, 14, titleColor);
        nextLabelX += MeasureText(title.c_str(), 14) + 10;

        if (!versionText.empty()) {
            DrawText(versionText.c_str(), nextLabelX, (int)rect.y + 8, 11, theme.textSecondary);
            nextLabelX += MeasureText(versionText.c_str(), 11) + 10;
        }
        if (!restartText.empty()) {
            DrawText(restartText.c_str(), nextLabelX, (int)rect.y + 8, 11, { 220, 170, 60, 255 });
        }

        if (!tagText.empty()) {
            DrawText(tagText.c_str(), (int)(rect.x + rect.width - MeasureText(tagText.c_str(), 11) - 8),
                      (int)rect.y + 8, 11, tagColor);
        }

        if (!descriptionText.empty()) {
            DrawText(descriptionText.c_str(), (int)rect.x + 8 + 26, (int)rect.y + 24, 11, theme.textSecondary);
        }
        if (!depsText.empty()) {
            DrawText(depsText.c_str(), (int)rect.x + 8 + 26, (int)rect.y + 40, 11, theme.textSecondary);
        }
    }
};

class CatalogRowElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "CatalogRowElement"; }
    std::string GetObjectKind() const override { return "CatalogRowElement"; }

    std::string label;

protected:
    void OnDrawSelf(Rectangle rect, float) override {
        const UIPalette& theme = UITheme::Get();
        DrawText(label.c_str(), (int)rect.x + 2, (int)rect.y + 4, 11, theme.textPrimary);
    }
};

}

void ModuleBrowserPanel::Initialize() {
    name = "Modules";
    ModuleManager::Get().ScanManifests();
}

void ModuleBrowserPanel::EnsureListChrome() {
    if (listChrome) return;
    listChrome = new Widget();
    auto* root = new VerticalBoxElement();
    root->scrollEnabled = true;
    root->childSpacing = 4.0f;
    listChrome->SetRoot(root);
}

void ModuleBrowserPanel::EnsureControlsChrome() {
    if (controlsChrome) return;
    controlsChrome = new Widget();
    auto* root = new WidgetElement();
    controlsChrome->SetRoot(root);
}

void ModuleBrowserPanel::EnsureRepoChrome() {
    if (repoChrome) return;
    repoChrome = new Widget();
    auto* root = new WidgetElement();
    repoChrome->SetRoot(root);

    const UIPalette& theme = UITheme::Get();
    repoTextField = new TextElement();
    repoTextField->editable = true;
    repoTextField->fontSize = 12.0f;
    repoTextField->backgroundColor = theme.fieldBg;
    repoTextField->borderColor = theme.fieldBorder;
    repoTextField->editingBorderColor = theme.accent;
    root->AddChild(repoTextField);
}


void ModuleBrowserPanel::Render() {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(bounds, theme.panelBg);
    DrawRectangleLinesEx(bounds, 1, theme.panelBorder);

    EnsureListChrome();
    EnsureControlsChrome();

    if (rebuildRunning) {
        LoadingPanel::Get().SetStatusLine(rebuildTask.GetLatestLine());
        bool taskSuccess = false;
        std::string taskMessage;
        if (rebuildTask.PollCompletion(taskSuccess, taskMessage)) {
            rebuildRunning = false;
            LoadingPanel::Get().Hide();
            if (taskSuccess) {
                statusMessage = "Rebuild complete. Restarting...";
                BuildRunner::Get().TriggerRelaunch();
            } else {
                statusMessage = "Build failed. See log for details.";
                if (logPanel) {
                    logPanel->AddLog("[Build Error] " + taskMessage, LogLevel::Error);
                    logPanel->AddLog(rebuildTask.GetFullLog());
                }
            }
        }
    }

    float applyBarHeight = pendingToggles.empty() ? 0.0f : 32.0f;
    float importBarHeight = inputMode == ModuleBrowserPanel::InputMode::None ? 26.0f : 56.0f;
    Rectangle importBar = { bounds.x + 8, bounds.y + bounds.height - importBarHeight - 6,
                             bounds.width - 16, importBarHeight };
    Rectangle applyBar = { bounds.x + 8, importBar.y - applyBarHeight, bounds.width - 16, applyBarHeight - 6 };

    Rectangle listArea = { bounds.x, bounds.y + 4, bounds.width,
                            bounds.height - importBarHeight - applyBarHeight - 16 };

    const auto& entries = ModuleManager::Get().GetEntries();
    float rowHeight = 58.0f;

    if (entries.size() != builtListCount) {
        VerticalBoxElement* root = static_cast<VerticalBoxElement*>(listChrome->rootElement);
        root->ClearChildren();
        builtListCount = entries.size();

        for (const ModuleEntry& entry : entries) {
            auto* row = new ModuleRowElement();
            row->size = { 0.0f, rowHeight };
            row->selectable = false;

            std::string moduleName = EntryDisplayName(entry);
            bool liveEnabled = entry.instance && entry.instance->IsEnabled();
            bool pendingRebuild = !entry.instance;

            row->title = moduleName;
            if (entry.hasManifest && !entry.manifest.version.empty()) {
                row->versionText = "v" + entry.manifest.version;
            }
            if (pendingRebuild) {
                row->tagText = "Pending Rebuild";
                row->tagColor = { 220, 170, 60, 255 };
            } else if (entry.hasManifest) {
                row->tagText = entry.manifest.category;
                row->tagColor = UITheme::Get().accent;
            }
            if (entry.hasManifest && !entry.manifest.description.empty()) {
                row->descriptionText = entry.manifest.description;
            }
            if (entry.hasManifest && !entry.manifest.dependencies.empty()) {
                std::string depsText = "Depends on: ";
                for (size_t i = 0; i < entry.manifest.dependencies.size(); i++) {
                    if (i > 0) depsText += ", ";
                    depsText += entry.manifest.dependencies[i];
                }
                row->depsText = depsText;
            }

            if (pendingRebuild) {
                row->hasCheckbox = false;
            } else {
                auto* box = new CheckBoxElement();
                box->checked = liveEnabled;
                box->onToggled = [this, moduleName, liveEnabled](bool toggled) {
                    if (importMenu.IsOpen() || engineOrProjectPopup.IsOpen() || rebuildRunning) return;
                    bool needsRestart = ModuleManager::Get().NeedsRestartToApply(moduleName, toggled);
                    if (needsRestart) {
                        if (toggled == liveEnabled) pendingToggles.erase(moduleName);
                        else pendingToggles[moduleName] = toggled;
                    } else {
                        ModuleManager::Get().SetModuleEnabled(moduleName, toggled);
                        pendingToggles.erase(moduleName);
                    }
                };
                row->AddChild(box);
            }

            root->AddChild(row);
        }
    }

    for (WidgetElement* rowEl : static_cast<VerticalBoxElement*>(listChrome->rootElement)->GetChildren()) {
        auto* row = static_cast<ModuleRowElement*>(rowEl);
        if (row->hasCheckbox && !row->GetChildren().empty()) {
            auto pendingIt = pendingToggles.find(row->title);
            row->restartText = (pendingIt != pendingToggles.end()) ? "(restart to apply)" : "";
        }
    }

    float dt = GetFrameTime();
    listChrome->Tick(dt, listArea, 1.0f);
    WidgetInput::ProcessInput(listChrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    listChrome->Draw(listArea, 1.0f);

    WidgetElement* controlsRoot = controlsChrome->rootElement;
    controlsRoot->ClearChildren();

    if (!pendingToggles.empty()) {
        std::string label = "Apply " + std::to_string(pendingToggles.size()) +
            (pendingToggles.size() == 1 ? " Change" : " Changes") + " & Restart";
        auto* btn = new ButtonElement();
        btn->size = { applyBar.width, applyBar.height };
        btn->defaultColor = theme.buttonBg;
        btn->hoveredColor = theme.buttonHover;
        btn->pressedColor = theme.buttonPressed;
        btn->onActivated = [this]() {
            if (!engineOrProjectPopup.IsOpen() && !rebuildRunning) ApplyPendingToggles();
        };
        auto* lbl = new TextElement();
        lbl->text = label;
        lbl->fontSize = 11.0f;
        lbl->anchor = { 0.5f, 0.5f };
        lbl->pivot = { 0.5f, 0.5f };
        btn->AddChild(lbl);
        controlsRoot->AddChild(btn);
        btn->offset = { applyBar.x - bounds.x, applyBar.y - bounds.y };
    }

    switch (inputMode) {
        case ModuleBrowserPanel::InputMode::None:            RenderImportBar();               break;
        case ModuleBrowserPanel::InputMode::RepositoryEntry: RenderRepositoryEntry(importBar); break;
        case ModuleBrowserPanel::InputMode::CatalogList:     RenderCatalogList(importBar);    break;
    }

    dt = GetFrameTime();
    controlsChrome->Tick(dt, bounds, 1.0f);
    WidgetInput::ProcessInput(controlsChrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    controlsChrome->Draw(bounds, 1.0f);

    int engineOrProjectChoice = -1;
    if (engineOrProjectPopup.Render(engineOrProjectChoice)) {
        if (engineOrProjectChoice == 0) {
            std::string engineDir = ModuleManager::GetEngineModulesRoot();
            std::string newDest, relocateError;
            if (ModuleImporter::RelocateModule(pendingRelocateDir, engineDir, newDest, relocateError)) {
                statusMessage = "Added to the engine.";
            } else {
                statusMessage = "Kept in this project (" + relocateError + ")";
            }
        }
        pendingRelocateDir.clear();
        FinishImport(true, "");
    }

    if (!statusMessage.empty()) {
        DrawText(statusMessage.c_str(), (int)bounds.x + 8, (int)(applyBarHeight > 0 ? applyBar.y - 16 : importBar.y - 16), 11, theme.success);
    }
}

void ModuleBrowserPanel::RenderImportBar() {
    float importBtnHeight = 26.0f;
    Rectangle importBtn = { bounds.x + 8, bounds.y + bounds.height - importBtnHeight - 6,
                             bounds.width - 16, importBtnHeight };

    auto* btn = new ButtonElement();
    btn->size = { importBtn.width, importBtn.height };
    btn->offset = { importBtn.x - bounds.x, importBtn.y - bounds.y };
    const UIPalette& theme = UITheme::Get();
    btn->defaultColor = theme.buttonBg;
    btn->hoveredColor = theme.buttonHover;
    btn->pressedColor = theme.buttonPressed;
    btn->onActivated = [this, importBtn]() {
        if (engineOrProjectPopup.IsOpen()) return;
        importMenu.Open({ importBtn.x, importBtn.y - 3 * 22.0f - 4 }, importBtn.width,
            { {"Import from Disk..."}, {"Import from Repository..."}, {"Import from Catalog..."} });
    };
    auto* lbl = new TextElement();
    lbl->text = "Import Module...";
    lbl->fontSize = 11.0f;
    lbl->anchor = { 0.5f, 0.5f };
    lbl->pivot = { 0.5f, 0.5f };
    btn->AddChild(lbl);
    controlsChrome->rootElement->AddChild(btn);

    int choiceIndex = -1;
    if (importMenu.Render(choiceIndex)) {
        if (choiceIndex == 0) ImportFromDisk();
        else if (choiceIndex == 1) BeginRepositoryEntry();
        else if (choiceIndex == 2) BeginCatalogBrowse();
    }
}

void ModuleBrowserPanel::RenderRepositoryEntry(Rectangle area) {
    const UIPalette& theme = UITheme::Get();

    DrawText("owner/repo", (int)area.x, (int)area.y, 11, theme.textSecondary);

    Rectangle textBox = { area.x, area.y + 16, area.width - 74, 26 };
    Rectangle confirmBtn = { textBox.x + textBox.width + 4, textBox.y, 34, 26 };
    Rectangle cancelBtn  = { confirmBtn.x + 38, textBox.y, 34, 26 };

    WidgetElement* root = controlsChrome->rootElement;

    EnsureRepoChrome();
    repoTextField->size = { textBox.width, textBox.height };
    repoTextField->offset = { textBox.x - bounds.x, textBox.y - bounds.y };
    if (!repoTextField->isEditing) repoTextField->StartEditing();

    float dt = GetFrameTime();
    repoChrome->Tick(dt, bounds, 1.0f);
    WidgetInput::ProcessInput(repoChrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    repoChrome->Draw(bounds, 1.0f);

    auto* addBtn = new ButtonElement();
    addBtn->size = { confirmBtn.width, confirmBtn.height };
    addBtn->offset = { confirmBtn.x - bounds.x, confirmBtn.y - bounds.y };
    addBtn->defaultColor = theme.buttonBg;
    addBtn->hoveredColor = theme.buttonHover;
    addBtn->onActivated = [this]() {
        if (!repoTextField || repoTextField->text.empty()) return;
        std::string ownerRepo = repoTextField->text;
        repoTextField->text.clear();
        inputMode = ModuleBrowserPanel::InputMode::None;
        ConfirmRepositoryEntry(ownerRepo);
    };
    auto* addLbl = new TextElement();
    addLbl->text = "Add";
    addLbl->fontSize = 11.0f;
    addLbl->anchor = { 0.5f, 0.5f };
    addLbl->pivot = { 0.5f, 0.5f };
    addBtn->AddChild(addLbl);
    root->AddChild(addBtn);

    auto* cancelBtnEl = new ButtonElement();
    cancelBtnEl->size = { cancelBtn.width, cancelBtn.height };
    cancelBtnEl->offset = { cancelBtn.x - bounds.x, cancelBtn.y - bounds.y };
    cancelBtnEl->defaultColor = theme.buttonBg;
    cancelBtnEl->hoveredColor = theme.buttonHover;
    cancelBtnEl->onActivated = [this]() {
        if (repoTextField) repoTextField->text.clear();
        inputMode = ModuleBrowserPanel::InputMode::None;
    };
    auto* cancelLbl = new TextElement();
    cancelLbl->text = "X";
    cancelLbl->fontSize = 11.0f;
    cancelLbl->anchor = { 0.5f, 0.5f };
    cancelLbl->pivot = { 0.5f, 0.5f };
    cancelBtnEl->AddChild(cancelLbl);
    root->AddChild(cancelBtnEl);
}

void ModuleBrowserPanel::RenderCatalogList(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    WidgetElement* root = controlsChrome->rootElement;

    if (catalogLoading) {
        DrawText("Loading catalog...", (int)area.x, (int)area.y + 4, 12, theme.textSecondary);
        return;
    }

    if (!catalogError.empty()) {
        DrawText(catalogError.c_str(), (int)area.x, (int)area.y + 4, 11, theme.danger);
        Rectangle closeBtn = { area.x + area.width - 60, area.y, 60, 22 };
        auto* btn = new ButtonElement();
        btn->size = { closeBtn.width, closeBtn.height };
        btn->offset = { closeBtn.x - bounds.x, closeBtn.y - bounds.y };
        btn->defaultColor = theme.buttonBg;
        btn->hoveredColor = theme.buttonHover;
        btn->onActivated = [this]() { inputMode = ModuleBrowserPanel::InputMode::None; };
        auto* lbl = new TextElement();
        lbl->text = "Close";
        lbl->fontSize = 11.0f;
        lbl->anchor = { 0.5f, 0.5f };
        lbl->pivot = { 0.5f, 0.5f };
        btn->AddChild(lbl);
        root->AddChild(btn);
        return;
    }

    float rowH = 22.0f;
    float y = area.y;
    for (size_t i = 0; i < catalogEntries.size(); i++) {
        const CatalogEntry& e = catalogEntries[i];
        std::string label = e.name + " - " + e.description;
        if (e.engineModule) label += " [Engine]";

        auto* labelEl = new CatalogRowElement();
        labelEl->label = label;
        labelEl->selectable = false;
        labelEl->size = { area.width - 64, rowH };
        labelEl->offset = { area.x - bounds.x, y - bounds.y };
        root->AddChild(labelEl);

        Rectangle addBtn = { area.x + area.width - 58, y, 58, rowH - 2 };
        auto* btn = new ButtonElement();
        btn->size = { addBtn.width, addBtn.height };
        btn->offset = { addBtn.x - bounds.x, addBtn.y - bounds.y };
        btn->defaultColor = theme.buttonBg;
        btn->hoveredColor = theme.buttonHover;
        std::string repo = e.repository;
        btn->onActivated = [this, repo]() {
            inputMode = ModuleBrowserPanel::InputMode::None;
            ConfirmRepositoryEntry(repo);
        };
        auto* lbl = new TextElement();
        lbl->text = "Add";
        lbl->fontSize = 11.0f;
        lbl->anchor = { 0.5f, 0.5f };
        lbl->pivot = { 0.5f, 0.5f };
        btn->AddChild(lbl);
        root->AddChild(btn);

        y += rowH;
    }

    Rectangle closeBtn = { area.x + area.width - 60, y + 4, 60, 22 };
    auto* closeBtnEl = new ButtonElement();
    closeBtnEl->size = { closeBtn.width, closeBtn.height };
    closeBtnEl->offset = { closeBtn.x - bounds.x, closeBtn.y - bounds.y };
    closeBtnEl->defaultColor = theme.buttonBg;
    closeBtnEl->hoveredColor = theme.buttonHover;
    closeBtnEl->onActivated = [this]() { inputMode = ModuleBrowserPanel::InputMode::None; };
    auto* closeLbl = new TextElement();
    closeLbl->text = "Close";
    closeLbl->fontSize = 11.0f;
    closeLbl->anchor = { 0.5f, 0.5f };
    closeLbl->pivot = { 0.5f, 0.5f };
    closeBtnEl->AddChild(closeLbl);
    root->AddChild(closeBtnEl);
}

void ModuleBrowserPanel::ImportFromDisk() {
    std::string path = NativeFileDialog::OpenFile(
        "Import Module", "", "Module Manifest", "*.starmodule");
    if (path.empty()) return;

    std::string modulesDir = Project::Get().GetModulesDirectory();
    std::string destDir, error;
    bool success = ModuleImporter::CopyFromDisk(path, modulesDir, destDir, error);
    if (!success) {
        FinishImport(false, error);
        return;
    }
    HandleImportedModule(destDir);
}

void ModuleBrowserPanel::BeginRepositoryEntry() {
    inputMode = ModuleBrowserPanel::InputMode::RepositoryEntry;
}

void ModuleBrowserPanel::ConfirmRepositoryEntry(const std::string& ownerRepo) {
    statusMessage = "Cloning " + ownerRepo + "...";
    std::string modulesDir = Project::Get().GetModulesDirectory();
    std::string destDir, error;
    bool success = ModuleImporter::CloneFromRepository(ownerRepo, modulesDir, destDir, error);
    if (!success) {
        FinishImport(false, error);
        return;
    }
    HandleImportedModule(destDir);
}

void ModuleBrowserPanel::BeginCatalogBrowse() {
    inputMode = ModuleBrowserPanel::InputMode::CatalogList;
    catalogLoading = true;
    catalogError.clear();
    catalogEntries.clear();

    bool ok = ModuleImporter::FetchCatalog(catalogEntries, catalogError);
    catalogLoading = false;
    if (!ok && catalogError.empty()) catalogError = "Could not load the module catalog.";
}

void ModuleBrowserPanel::HandleImportedModule(const std::string& destDir) {
    ModuleManifest manifest;
    bool foundManifest = false;
    std::error_code ec;
    for (const auto& fileEntry : fs::directory_iterator(destDir, ec)) {
        if (fileEntry.path().extension() == ".starmodule") {
            foundManifest = ModuleManifest::LoadFromFile(fileEntry.path().string(), manifest);
            break;
        }
    }

    if (foundManifest && manifest.engineModule) {
        pendingRelocateDir = destDir;
        engineOrProjectPopup.Open({bounds.x + bounds.width * 0.5f - 140.0f, bounds.y + bounds.height * 0.5f}, 280.0f, {
            {"Add to the Engine", true, ""},
            {"Keep in This Project", true, ""},
        });
        return;
    }

    FinishImport(true, "");
}

void ModuleBrowserPanel::FinishImport(bool success, const std::string& message) {
    if (!success) {
        statusMessage = message.empty() ? "Import failed." : message;
        return;
    }

    ModuleManager::Get().ScanManifests();
    statusMessage = "Imported. Rebuilding...";
    StartRebuildAsync();
}

void ModuleBrowserPanel::StartRebuildAsync() {
    rebuildRunning = true;
    LoadingPanel::Get().Show("Rebuilding StarEngine");

    std::string buildDir = BuildRunner::GetDefaultBuildDirectory();
    std::string config = BuildRunner::GetDefaultBuildConfig();

    rebuildTask.Start([buildDir, config](const std::function<void(const std::string&)>& reportLine,
                                          std::string& outFinalMessage) {
        BuildResult result = BuildRunner::Get().RunBuild(buildDir, config, reportLine);
        outFinalMessage = result.success ? "" : (result.error.empty() ? result.output : result.error);
        return result.success;
    });
}

void ModuleBrowserPanel::ApplyPendingToggles() {
    for (const auto& toggle : pendingToggles) {
        ModuleManager::Get().SetModuleEnabled(toggle.first, toggle.second);
    }
    pendingToggles.clear();

    Project::Get().Save();

    statusMessage = "Restarting...";
    BuildRunner::Get().TriggerRelaunch();
}
