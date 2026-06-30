#include "ModuleBrowserPanel.h"
#include "NativeFileDialog.h"
#include "Core/Module.h"
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

    if (entry.hasManifest && !entry.manifest.version.empty()) {
        std::string versionText = "v" + entry.manifest.version;
        DrawText(versionText.c_str(), (int)checkRect.x + 26 + MeasureText(moduleName.c_str(), 14) + 10,
                  (int)row.y + 8, 11, {130, 130, 140, 255});
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

    float importBtnHeight = 26.0f;
    Rectangle importBtn = { bounds.x + 8, bounds.y + bounds.height - importBtnHeight - 6,
                             bounds.width - 16, importBtnHeight };

    Rectangle listArea = { bounds.x, bounds.y + 4, bounds.width, bounds.height - importBtnHeight - 16 };

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

    if (GuiButton(importBtn, "Import Module...")) {
        ImportModule();
    }

    if (!statusMessage.empty()) {
        DrawText(statusMessage.c_str(), (int)bounds.x + 8, (int)(importBtn.y - 16), 11, {180, 200, 120, 255});
    }
}

void ModuleBrowserPanel::ImportModule() {
    std::string path = NativeFileDialog::OpenFile(
        "Import Module", "", "Module Manifest", "*.starmodule");
    if (path.empty()) return;

    ModuleManifest manifest;
    if (!ModuleManifest::LoadFromFile(path, manifest)) {
        statusMessage = "Failed to read manifest: " + path;
        return;
    }

    fs::path sourceDir = fs::path(path).parent_path();
    fs::path destDir = fs::path(ModuleManager::Get().GetModulesSourceRoot()) / manifest.name;

    if (fs::exists(destDir) && fs::equivalent(sourceDir, destDir)) {
        statusMessage = "Module already lives in src/Modules.";
        return;
    }

    std::error_code ec;
    fs::create_directories(destDir.parent_path(), ec);
    fs::copy(sourceDir, destDir, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

    if (ec) {
        statusMessage = "Import failed: " + ec.message();
        return;
    }

    ModuleManager::Get().ScanManifests();
    statusMessage = "Imported " + manifest.name + ". Rebuild the engine to compile it in.";
}
