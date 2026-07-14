#include "ModuleBrowserPanel.h"
#include "NativeFileDialog.h"
#include "Core/Module.h"
#include "Core/BuildRunner.h"
#include "Core/Project.h"
#include "Core/ModuleImporter.h"
#include "LogPanel.h"
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
    Rectangle row = { bounds.x + 4, y, bounds.width - 8, rowHeight };
    DrawRectangleRec(row, {44, 44, 48, 255});
    DrawRectangleLinesEx(row, 1, {60, 60, 66, 255});

    std::string moduleName = EntryDisplayName(entry);
    bool enabled = entry.instance && entry.instance->IsEnabled();
    bool pendingRebuild = !entry.instance;

    Rectangle checkRect = { row.x + 8, row.y + 8, 18, 18 };
    if (pendingRebuild) {
        DrawRectangleLinesEx(checkRect, 1, {90, 90, 96, 255});
    } else {
        bool toggled = enabled;
        if (GuiCheckBox(checkRect, "", &toggled)) {
            ModuleManager::Get().SetModuleEnabled(moduleName, toggled);
        }
    }

    Color titleColor = enabled || pendingRebuild ? WHITE : Color{120, 120, 126, 255};
    DrawText(moduleName.c_str(), (int)checkRect.x + 26, (int)row.y + 6, 14, titleColor);

    int nextLabelX = (int)checkRect.x + 26 + MeasureText(moduleName.c_str(), 14) + 10;
    if (entry.hasManifest && !entry.manifest.version.empty()) {
        std::string versionText = "v" + entry.manifest.version;
        DrawText(versionText.c_str(), nextLabelX, (int)row.y + 8, 11, {130, 130, 140, 255});
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
        DrawText(tag.c_str(), (int)(row.x + row.width - MeasureText(tag.c_str(), 11) - 8), (int)row.y + 8, 11, {120, 160, 220, 255});
    }

    if (entry.hasManifest && !entry.manifest.description.empty()) {
        DrawText(entry.manifest.description.c_str(), (int)checkRect.x + 26, (int)row.y + 24, 11, {150, 150, 158, 255});
    }

    if (entry.hasManifest && !entry.manifest.dependencies.empty()) {
        std::string depsText = "Depends on: ";
        for (size_t i = 0; i < entry.manifest.dependencies.size(); i++) {
            if (i > 0) depsText += ", ";
            depsText += entry.manifest.dependencies[i];
        }
        DrawText(depsText.c_str(), (int)checkRect.x + 26, (int)row.y + 40, 11, {130, 130, 140, 255});
    }
}

void ModuleBrowserPanel::Render() {
    DrawRectangleRec(bounds, {38, 38, 42, 255});
    DrawRectangleLinesEx(bounds, 1, {65, 65, 70, 255});

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

    if (!statusMessage.empty()) {
        DrawText(statusMessage.c_str(), (int)bounds.x + 8, (int)(importBar.y - 16), 11, {180, 200, 120, 255});
    }
}

void ModuleBrowserPanel::RenderImportBar() {
    float importBtnHeight = 26.0f;
    Rectangle importBtn = { bounds.x + 8, bounds.y + bounds.height - importBtnHeight - 6,
                             bounds.width - 16, importBtnHeight };

    if (GuiButton(importBtn, "Import Module...")) {
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
    Rectangle label = { area.x, area.y, area.width, 16 };
    DrawText("owner/repo", (int)label.x, (int)label.y, 11, {150, 150, 158, 255});

    Rectangle textBox = { area.x, area.y + 16, area.width - 74, 26 };
    if (CheckCollisionPointRec(GetMousePosition(), textBox) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        repoInputEditing = true;
    }
    GuiTextBox(textBox, repoInputBuffer, sizeof(repoInputBuffer), repoInputEditing);

    Rectangle confirmBtn = { textBox.x + textBox.width + 4, textBox.y, 34, 26 };
    Rectangle cancelBtn  = { confirmBtn.x + 38, textBox.y, 34, 26 };

    bool confirm = GuiButton(confirmBtn, "Add");
    if ((confirm || IsKeyPressed(KEY_ENTER)) && repoInputBuffer[0] != '\0') {
        std::string ownerRepo(repoInputBuffer);
        repoInputBuffer[0] = '\0';
        repoInputEditing = false;
        inputMode = InputMode::None;
        ConfirmRepositoryEntry(ownerRepo);
    }
    if (GuiButton(cancelBtn, "X")) {
        repoInputBuffer[0] = '\0';
        repoInputEditing = false;
        inputMode = InputMode::None;
    }
}

void ModuleBrowserPanel::RenderCatalogList(Rectangle area) {
    if (catalogLoading) {
        DrawText("Loading catalog...", (int)area.x, (int)area.y + 4, 12, {180, 180, 188, 255});
        return;
    }

    if (!catalogError.empty()) {
        DrawText(catalogError.c_str(), (int)area.x, (int)area.y + 4, 11, {220, 120, 120, 255});
        Rectangle closeBtn = { area.x + area.width - 60, area.y, 60, 22 };
        if (GuiButton(closeBtn, "Close")) inputMode = InputMode::None;
        return;
    }

    float rowH = 22.0f;
    float y = area.y;
    for (size_t i = 0; i < catalogEntries.size(); i++) {
        const CatalogEntry& e = catalogEntries[i];
        Rectangle row = { area.x, y, area.width - 64, rowH };
        std::string label = e.name + " - " + e.description;
        DrawText(label.c_str(), (int)row.x + 2, (int)row.y + 4, 11, WHITE);

        Rectangle addBtn = { area.x + area.width - 58, y, 58, rowH - 2 };
        if (GuiButton(addBtn, "Add")) {
            inputMode = InputMode::None;
            ConfirmRepositoryEntry(e.repository);
            return;
        }
        y += rowH;
    }

    Rectangle closeBtn = { area.x + area.width - 60, y + 4, 60, 22 };
    if (GuiButton(closeBtn, "Close")) inputMode = InputMode::None;
}

void ModuleBrowserPanel::ImportFromDisk() {
    std::string path = NativeFileDialog::OpenFile(
        "Import Module", "", "Module Manifest", "*.starmodule");
    if (path.empty()) return;

    std::string modulesDir = Project::Get().GetModulesDirectory();
    std::string error;
    bool success = ModuleImporter::CopyFromDisk(path, modulesDir, error);
    FinishImport(success, error);
}

void ModuleBrowserPanel::BeginRepositoryEntry() {
    inputMode = InputMode::RepositoryEntry;
    repoInputEditing = true;
}

void ModuleBrowserPanel::ConfirmRepositoryEntry(const std::string& ownerRepo) {
    statusMessage = "Cloning " + ownerRepo + "...";
    std::string modulesDir = Project::Get().GetModulesDirectory();
    std::string error;
    bool success = ModuleImporter::CloneFromRepository(ownerRepo, modulesDir, error);
    FinishImport(success, error);
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