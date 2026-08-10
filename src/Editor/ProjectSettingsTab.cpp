#include "ProjectSettingsTab.h"
#include "PropertyFields.h"
#include "MenuBar.h"
#include "Core/Project.h"
#include "Core/NamedListRegistry.h"
#include "Core/SaveFileRegistry.h"
#include "Core/RenderSettings.h"
#include "Core/ShadingTechniqueRegistry.h"
#include "Core/ShadingModel.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

void ProjectSettingsTab::Initialize() {
    title     = "Project Settings";
    permanent = false;
    switcher.SetCategories({ "General", "Maps", "Rendering", "Save Files" });
    RefreshFromProject();
}

void ProjectSettingsTab::RefreshFromProject() {
    snprintf(renderScaleBuf, sizeof(renderScaleBuf), "%.2f", RenderSettings::Get().GetRenderScale() * 100.0f);
    activeField = 0;
    newSaveFileBuf[0] = '\0';
    newSaveFileName.clear();
    mapPicker.Close();
    rendererPopup.Close();
    shadingTypePopup.Close();
}

static std::string MapDisplayName(const std::string& path) {
    if (path.empty()) return "None";
    return fs::path(path).stem().string();
}

void ProjectSettingsTab::Render(float x, float y, float width, float height) {
    Rectangle content = switcher.Render({x, y, width, height});

    switch (switcher.Selected()) {
        case 0: RenderGeneralCategory(content);   break;
        case 1: RenderMapsCategory(content);       break;
        case 2: RenderRenderingCategory(content);  break;
        case 3: RenderSaveFilesCategory(content);  break;
        default: break;
    }
}

void ProjectSettingsTab::RenderGeneralCategory(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    ProjectSettings& settings = Project::Get().GetSettings();

    float fx = area.x + 16;
    float fy = area.y + 16;
    float fw = area.width - 32 > 420.0f ? 420.0f : area.width - 32;

    DrawText("General", (int)fx, (int)fy, 14, theme.sectionHeader);
    fy += 24;

    DrawText("Project Name", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle nameRow = {fx, fy, fw, 20};
    DrawRectangleRec(nameRow, theme.fieldBg);
    DrawRectangleLinesEx(nameRow, 1, theme.fieldBorder);
    DrawText(settings.name.c_str(), (int)nameRow.x + 6, (int)nameRow.y + 4, 12, theme.textPrimary);
    fy += 34;
}

void ProjectSettingsTab::RenderMapsCategory(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    ProjectSettings& settings = Project::Get().GetSettings();
    Vector2 mp = GetMousePosition();
    const bool anyPopupOpen = mapPicker.IsOpen();

    float fx = area.x + 16;
    float fy = area.y + 16;
    float fw = area.width - 32 > 420.0f ? 420.0f : area.width - 32;

    DrawText("Maps", (int)fx, (int)fy, 14, theme.sectionHeader);
    fy += 24;

    DrawText("Default Map", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle mapRow = {fx, fy, fw, 20};
    DrawRectangleRec(mapRow, theme.fieldBg);
    DrawRectangleLinesEx(mapRow, 1, theme.fieldBorder);
    DrawText(MapDisplayName(settings.defaultMapPath).c_str(), (int)mapRow.x + 6, (int)mapRow.y + 4, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, mapRow) && !anyPopupOpen) {
        mapPicker.Open({mapRow.x, mapRow.y + mapRow.height}, ".starmap");
    }
    fy += 20;
    DrawText("Map loaded on project open and used as the packaged game's startup map.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;
    std::string pickedMap;
    if (mapPicker.Render(pickedMap)) settings.defaultMapPath = pickedMap;
}

void ProjectSettingsTab::RenderRenderingCategory(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    Vector2 mp = GetMousePosition();
    const bool anyPopupOpen = rendererPopup.IsOpen() || shadingTypePopup.IsOpen() || lightingQualityPopup.IsOpen();

    float fx = area.x + 16;
    float fy = area.y + 16;
    float fw = area.width - 32 > 420.0f ? 420.0f : area.width - 32;

    ProjectSettings& settings = Project::Get().GetSettings();

    DrawText("Rendering", (int)fx, (int)fy, 14, theme.sectionHeader);
    fy += 24;

    DrawText("Renderer", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle rendererRow = {fx, fy, fw, 20};
    DrawRectangleRec(rendererRow, theme.fieldBg);
    DrawRectangleLinesEx(rendererRow, 1, theme.fieldBorder);
    std::string rendererLabel = settings.renderer == GameRenderer::Software
        ? "Software (CPU, no GPU required)" : "Hardware (OpenGL)";
    DrawText(rendererLabel.c_str(), (int)rendererRow.x + 6, (int)rendererRow.y + 4, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, rendererRow) && !anyPopupOpen) {
        rendererPopup.Open({rendererRow.x, rendererRow.y + rendererRow.height}, fw, {
            {"Hardware (OpenGL)", true, ""},
            {"Software (CPU, no GPU required)", true, ""},
        });
    }
    fy += 20;
    DrawText("Only affects packaged games; the editor always uses Hardware.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;
    int rendererChoice = 0;
    if (rendererPopup.Render(rendererChoice)) {
        settings.renderer = rendererChoice == 1 ? GameRenderer::Software : GameRenderer::Hardware;
    }

    DrawText("Shading Type", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle shadingRow = {fx, fy, fw, 20};
    DrawRectangleRec(shadingRow, theme.fieldBg);
    DrawRectangleLinesEx(shadingRow, 1, theme.fieldBorder);

    shadingOptionIds.clear();
    std::vector<ChoiceOption> shadingChoices;
    for (const ShadingTechnique& option : ShadingTechniqueRegistry::Get().GetAllOptions()) {
        shadingOptionIds.push_back(option.id);
        shadingChoices.push_back({option.displayName, true, ""});
    }

    const std::string& currentShadingId = RenderSettings::Get().GetShadingType();
    std::string shadingLabel = "Unlit";
    for (size_t i = 0; i < shadingOptionIds.size(); i++) {
        if (shadingOptionIds[i] == currentShadingId) {
            shadingLabel = shadingChoices[i].label;
            break;
        }
    }
    DrawText(shadingLabel.c_str(), (int)shadingRow.x + 6, (int)shadingRow.y + 4, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, shadingRow) && !anyPopupOpen) {
        shadingTypePopup.Open({shadingRow.x, shadingRow.y + shadingRow.height}, fw, shadingChoices);
    }
    fy += 20;
    DrawText("Unlit runs no lighting or shadow pass at all, the cheapest option. Other options come from enabled shadow modules.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;
    int shadingChoice = 0;
    if (shadingTypePopup.Render(shadingChoice) && shadingChoice >= 0 && shadingChoice < (int)shadingOptionIds.size()) {
        RenderSettings::Get().SetShadingType(shadingOptionIds[shadingChoice]);
    }

    bool lightingEnabled = ShadingModel::ShouldUseLitShading();
    Color lightingLabelColor = lightingEnabled ? theme.textSecondary : theme.buttonDisabledText;
    DrawText("Lighting Quality", (int)fx, (int)fy, 11, lightingLabelColor);
    fy += 14;
    Rectangle lightingRow = {fx, fy, fw, 20};
    DrawRectangleRec(lightingRow, theme.fieldBg);
    DrawRectangleLinesEx(lightingRow, 1, theme.fieldBorder);
    LightingQuality lightingQuality = RenderSettings::Get().GetLightingQuality();
    std::string lightingLabel = lightingQuality == LightingQuality::PerPixel ? "Per Pixel" : "Per Vertex";
    DrawText(lightingLabel.c_str(), (int)lightingRow.x + 6, (int)lightingRow.y + 4, 12,
              lightingEnabled ? theme.textPrimary : theme.buttonDisabledText);
    if (lightingEnabled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, lightingRow) && !anyPopupOpen) {
        lightingQualityPopup.Open({lightingRow.x, lightingRow.y + lightingRow.height}, fw, {
            {"Per Vertex", true, ""},
            {"Per Pixel", true, ""},
        });
    }
    fy += 20;
    DrawText("Per Vertex is cheap but can look faceted on low-poly meshes. Per Pixel costs more, looks smoother.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;
    int lightingChoice = 0;
    if (lightingQualityPopup.Render(lightingChoice)) {
        RenderSettings::Get().SetLightingQuality(lightingChoice == 1 ? LightingQuality::PerPixel : LightingQuality::PerVertex);
    }

    DrawText("Render Scale", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle scaleRow = {fx, fy, 100.0f, 20};
    float renderScalePct = RenderSettings::Get().GetRenderScale() * 100.0f;
    float prevRenderScalePct = renderScalePct;
    PropertyField::FloatBox(scaleRow, renderScaleBuf, sizeof(renderScaleBuf), 4, activeField, renderScalePct);
    DrawText("%", (int)scaleRow.x + 106, (int)scaleRow.y + 4, 12, theme.textSecondary);
    if (renderScalePct != prevRenderScalePct) {
        if (renderScalePct < 10.0f) renderScalePct = 10.0f;
        RenderSettings::Get().SetRenderScale(renderScalePct / 100.0f);
    }
    fy += 20;
    DrawText("Percentage of the window resolution the 3D scene renders at, upscaled with nearest-neighbor.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;
}

void ProjectSettingsTab::RenderSaveFilesCategory(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    float fx = area.x + 16;
    float fy = area.y + 16;
    float fw = area.width - 32 > 420.0f ? 420.0f : area.width - 32;

    DrawText("Save Files", (int)fx, (int)fy, 14, theme.sectionHeader);
    fy += 24;
    DrawText("Named save files variables and component fields can be assigned to.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 18;

    const std::vector<std::string>& saveFiles = NamedListRegistry::Get().GetList(SaveFileRegistry::Category());
    std::string removeName;
    for (const std::string& saveFileName : saveFiles) {
        Rectangle nameCol = {fx, fy, fw - 130, 20};
        Rectangle encRow  = {fx + fw - 128, fy, 18, 18};
        Rectangle delBtn  = {fx + fw - 20, fy, 20, 20};

        DrawRectangleRec(nameCol, theme.fieldBg);
        DrawRectangleLinesEx(nameCol, 1, theme.fieldBorder);
        DrawText(saveFileName.c_str(), (int)nameCol.x + 6, (int)nameCol.y + 4, 12, theme.textPrimary);

        bool encrypted = SaveFileRegistry::Get().IsEncrypted(saveFileName);
        if (PropertyField::BoolBox(encRow, encrypted)) {
            SaveFileRegistry::Get().SetEncrypted(saveFileName, encrypted);
        }
        DrawText("Enc", (int)encRow.x + 22, (int)encRow.y + 2, 10, theme.textSecondary);

        if (UIWidget::Button(delBtn, "x")) removeName = saveFileName;

        fy += 22;
    }
    if (!removeName.empty()) NamedListRegistry::Get().RemoveEntry(SaveFileRegistry::Category(), removeName);

    Rectangle newNameRow = {fx, fy, fw - 60, 20};
    Rectangle addBtn     = {fx + fw - 56, fy, 56, 20};
    PropertyField::StringBox(newNameRow, newSaveFileBuf, sizeof(newSaveFileBuf), 3, activeField, newSaveFileName,
                              true, "New save file name");
    if (UIWidget::Button(addBtn, "+ Add") && !newSaveFileName.empty()) {
        NamedListRegistry::Get().AddEntry(SaveFileRegistry::Category(), newSaveFileName);
        newSaveFileName.clear();
        newSaveFileBuf[0] = '\0';
    }
    fy += 30;
}

void ProjectSettingsTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { "Save Project" } },
        { "Edit",   { } },
        { "Window", { } },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}