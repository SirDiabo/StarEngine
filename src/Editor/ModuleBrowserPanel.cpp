#include "ModuleBrowserPanel.h"
#include "NativeFileDialog.h"
#include "Core/Module.h"
#include "Core/ModuleManifest.h"
#include "Core/BuildRunner.h"
#include "Core/Project.h"
#include "Core/ModuleImporter.h"
#include "LogPanel.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>

namespace fs = std::filesystem;

void ModuleBrowserPanel::Initialize() {
    name = "Modules";
    ModuleManager::Get().ScanManifests();
}

static std::string EntryDisplayName(const ModuleEntry& entry) {
    return entry.instance ? entry.instance->GetName() : entry.manifest.name;
}

void ModuleBrowserPanel::RenderRow(const ModuleEntry& entry, float y, float rowHeight) {
    const UIPalette& theme = UITheme::Get();

    Rectangle row = { bounds.x + 4, y, bounds.width - 8, rowHeight };
    DrawRectangleRec(row, theme.buttonBg);
    DrawRectangleLinesEx(row, 1, theme.buttonBorder);

    std::string moduleName = EntryDisplayName(entry);
    bool enabled = entry.instance && entry.instance->IsEnabled();
    bool pendingRebuild = !entry.instance;

    Rectangle checkRect = { row.x + 8, row.y + 8, 18, 18 };
    if (pendingRebuild) {
        DrawRectangleLinesEx(checkRect, 1, theme.buttonBorder);
    } else {
        bool toggled = enabled;
        if (GuiCheckBox(checkRect, "", &toggled) && !importMenu.IsOpen() && !engineOrProjectPopup.IsOpen()) {
            ModuleManager::Get().SetModuleEnabled(moduleName, toggled);
        }
    }

    Color titleColor = enabled || pendingRebuild ? theme.textPrimary : theme.buttonDisabledText;
    DrawText(moduleName.c_str(), (int)checkRect.x + 26, (int)row.y + 6, 14, titleColor);

    int nextLabelX = (int)checkRect.x + 26 + MeasureText(moduleName.c_str(), 14) + 10;
    if (entry.hasManifest && !entry.manifest.version.empty()) {
        std::string versionText = "v" + entry.manifest.version;
        DrawText(versionText.c_str(), nextLabelX, (int)row.y + 8, 11, theme.textSecondary);
        nextLabelX += MeasureText(versionText.c_str(), 11) + 10;
    }
    if (entry.hasManifest && entry.manifest.hasExternalDependencies) {
        DrawText("(external)", nextLabelX, (int)row.y + 8, 11, {160, 130, 200, 255});
    }

    if (pendingRebuild) {
        const char* tag = "Pending Rebuild";
        DrawText(tag, (int)(row.x + row.width - MeasureText(tag, 11) - 8), (int)row.y + 8, 11, {220, 170, 60, 255});
    } else if (entry.hasManifest) {
        std::string tag = entry.manifest.category;
        DrawText(tag.c_str(), (int)(row.x + row.width - MeasureText(tag.c_str(), 11) - 8), (int)row.y + 8, 11, theme.accent);
    }

    if (entry.hasManifest && !entry.manifest.description.empty()) {
        DrawText(entry.manifest.description.c_str(), (int)checkRect.x + 26, (int)row.y + 24, 11, theme.textSecondary);
    }

    if (entry.hasManifest && !entry.manifest.dependencies.empty()) {
        std::string depsText = "Depends on: ";
        for (size_t i = 0; i < entry.manifest.dependencies.size(); i++) {
            if (i > 0) depsText += ", ";
            depsText += entry.manifest.dependencies[i];
        }
        DrawText(depsText.c_str(), (int)checkRect.x + 26, (int)row.y + 40, 11, theme.textSecondary);
    }
}

void ModuleBrowserPanel::Render() {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(bounds);

    float importBarHeight = inputMode == InputMode::None ? 26.0f : 56.0f;
    Rectangle importBar = { bounds.x + 8, bounds.y + bounds.height - importBarHeight - 6,
                             bounds.width - 16, importBarHeight };

    Rectangle listArea = { bounds.x, bounds.y + 4, bounds.width, bounds.height - importBarHeight - 16 };

    const auto& entries = ModuleManager::Get().GetEntries();
    float rowHeight = 58.0f;
    float contentHeight = entries.size() * (rowHeight + 4.0f);

    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, listArea)) {
        scrollOffset -= GetMouseWheelMove() * 20.0f;
    }
    float maxScroll = (contentHeight > listArea.height) ? (contentHeight - listArea.height) : 0.0f;
    if (scrollOffset < 0.0f) scrollOffset = 0.0f;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;

    BeginScissorMode((int)listArea.x, (int)listArea.y, (int)listArea.width, (int)listArea.height);
    float y = listArea.y - scrollOffset;
    for (const ModuleEntry& entry : entries) {
        if (y + rowHeight >= listArea.y && y <= listArea.y + listArea.height) {
            RenderRow(entry, y, rowHeight);
        }
        y += rowHeight + 4.0f;
    }
    EndScissorMode();

    switch (inputMode) {
        case InputMode::None:            RenderImportBar();               break;
        case InputMode::RepositoryEntry: RenderRepositoryEntry(importBar); break;
        case InputMode::CatalogList:     RenderCatalogList(importBar);    break;
    }

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
        DrawText(statusMessage.c_str(), (int)bounds.x + 8, (int)(importBar.y - 16), 11, theme.success);
    }
}

void ModuleBrowserPanel::RenderImportBar() {
    float importBtnHeight = 26.0f;
    Rectangle importBtn = { bounds.x + 8, bounds.y + bounds.height - importBtnHeight - 6,
                             bounds.width - 16, importBtnHeight };

    if (UIWidget::Button(importBtn, "Import Module...") && !engineOrProjectPopup.IsOpen()) {
        importMenu.Open({importBtn.x, importBtn.y - 3 * 22.0f - 4}, importBtn.width,
            { {"Import from Disk..."}, {"Import from Repository..."}, {"Import from Catalog..."} });
    }

    int choiceIndex = -1;
    if (importMenu.Render(choiceIndex)) {
        if (choiceIndex == 0) ImportFromDisk();
        else if (choiceIndex == 1) BeginRepositoryEntry();
        else if (choiceIndex == 2) BeginCatalogBrowse();
    }
}

void ModuleBrowserPanel::RenderRepositoryEntry(Rectangle area) {
    const UIPalette& theme = UITheme::Get();

    Rectangle label = { area.x, area.y, area.width, 16 };
    DrawText("owner/repo", (int)label.x, (int)label.y, 11, theme.textSecondary);

    Rectangle textBox = { area.x, area.y + 16, area.width - 74, 26 };
    if (CheckCollisionPointRec(GetMousePosition(), textBox) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        repoInputEditing = true;
    }
    GuiTextBox(textBox, repoInputBuffer, sizeof(repoInputBuffer), repoInputEditing);

    Rectangle confirmBtn = { textBox.x + textBox.width + 4, textBox.y, 34, 26 };
    Rectangle cancelBtn  = { confirmBtn.x + 38, textBox.y, 34, 26 };

    bool confirm = UIWidget::Button(confirmBtn, "Add");
    if ((confirm || IsKeyPressed(KEY_ENTER)) && repoInputBuffer[0] != '\0') {
        std::string ownerRepo(repoInputBuffer);
        repoInputBuffer[0] = '\0';
        repoInputEditing = false;
        inputMode = InputMode::None;
        ConfirmRepositoryEntry(ownerRepo);
    }
    if (UIWidget::Button(cancelBtn, "X")) {
        repoInputBuffer[0] = '\0';
        repoInputEditing = false;
        inputMode = InputMode::None;
    }
}

void ModuleBrowserPanel::RenderCatalogList(Rectangle area) {
    const UIPalette& theme = UITheme::Get();

    if (catalogLoading) {
        DrawText("Loading catalog...", (int)area.x, (int)area.y + 4, 12, theme.textSecondary);
        return;
    }

    if (!catalogError.empty()) {
        DrawText(catalogError.c_str(), (int)area.x, (int)area.y + 4, 11, theme.danger);
        Rectangle closeBtn = { area.x + area.width - 60, area.y, 60, 22 };
        if (UIWidget::Button(closeBtn, "Close")) inputMode = InputMode::None;
        return;
    }

    float rowH = 22.0f;
    float y = area.y;
    for (size_t i = 0; i < catalogEntries.size(); i++) {
        const CatalogEntry& e = catalogEntries[i];
        Rectangle row = { area.x, y, area.width - 64, rowH };
        std::string label = e.name + " - " + e.description;
        if (e.engineModule) label += " [Engine]";
        DrawText(label.c_str(), (int)row.x + 2, (int)row.y + 4, 11, theme.textPrimary);

        Rectangle addBtn = { area.x + area.width - 58, y, 58, rowH - 2 };
        if (UIWidget::Button(addBtn, "Add")) {
            inputMode = InputMode::None;
            ConfirmRepositoryEntry(e.repository);
            return;
        }
        y += rowH;
    }

    Rectangle closeBtn = { area.x + area.width - 60, y + 4, 60, 22 };
    if (UIWidget::Button(closeBtn, "Close")) inputMode = InputMode::None;
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
    inputMode = InputMode::RepositoryEntry;
    repoInputEditing = true;
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
    inputMode = InputMode::CatalogList;
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

    BuildResult result = BuildRunner::Get().RunBuild(
        BuildRunner::GetDefaultBuildDirectory(), BuildRunner::GetDefaultBuildConfig());
    if (result.success) {
        statusMessage = "Rebuild complete. Restarting...";
        BuildRunner::Get().TriggerRelaunch();
    } else {
        statusMessage = "Build failed. See log for details.";
        if (logPanel) {
            logPanel->AddLog("[Build Error] " + result.error, LogLevel::Error);
            logPanel->AddLog(result.output);
        }
    }
}