#include "ProjectSettingsTab.h"
#include "MenuBar.h"
#include "Core/Project.h"
#include "Core/NamedListRegistry.h"
#include "Core/SaveFileRegistry.h"
#include "Core/RenderSettings.h"
#include "Core/ShadingTechniqueRegistry.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/CollisionLayers.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

static bool InlineButton(Rectangle r, const char* label) {
    const UIPalette& theme = UITheme::Get();
    bool hovered = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, hovered ? theme.buttonHover : theme.buttonBg);
    DrawRectangleLinesEx(r, 1, theme.buttonBorder);
    int tw = MeasureText(label, 11);
    DrawText(label, (int)(r.x + (r.width - tw) / 2.0f), (int)(r.y + (r.height - 11) / 2.0f), 11, theme.textPrimary);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static bool InlineBoolBox(Rectangle r, bool& value) {
    const UIPalette& theme = UITheme::Get();
    bool hovered = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, hovered ? theme.surfaceHover : theme.fieldBg);
    DrawRectangleLinesEx(r, 1, theme.fieldBorder);
    if (value) {
        float pad = r.width * 0.22f;
        DrawRectangleRec({r.x + pad, r.y + pad, r.width - pad * 2, r.height - pad * 2}, theme.accent);
    }
    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        value = !value;
        return true;
    }
    return false;
}

void ProjectSettingsTab::Initialize() {
    title     = "Project Settings";
    permanent = false;
    switcher.SetCategories({ "General", "Maps", "Rendering", "Save Files", "Collision" });
    RefreshFromProject();
}

void ProjectSettingsTab::RefreshFromProject() {
    snprintf(renderScaleBuf, sizeof(renderScaleBuf), "%.2f", RenderSettings::Get().GetRenderScale() * 100.0f);
    activeField = 0;
    migrationsLoaded = false;
    newSaveFileBuf[0] = '\0';
    newSaveFileName.clear();
    mapPicker.Close();
    rendererPopup.Close();
    shadingTypePopup.Close();
    moduleSettingsPopupOpen = false;
    moduleSettingsSectionHeight = 0.0f;
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
        case 4: RenderCollisionLayersCategory(content); break;
        default: break;
    }
}

void ProjectSettingsTab::RenderGeneralCategory(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    ProjectSettings& settings = Project::Get().GetSettings();

    float fx = area.x + 16;
    float fy = area.y + 16;
    float fw = area.width - 32 > 640.0f ? 640.0f : area.width - 32;

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
    const bool anyPopupOpen = mapPicker.IsOpen() || gameMapPicker.IsOpen();

    float fx = area.x + 16;
    float fy = area.y + 16;
    float fw = area.width - 32 > 640.0f ? 640.0f : area.width - 32;

    DrawText("Maps", (int)fx, (int)fy, 14, theme.sectionHeader);
    fy += 24;

    DrawText("Editor Default Map", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle mapRow = {fx, fy, fw, 20};
    DrawRectangleRec(mapRow, theme.fieldBg);
    DrawRectangleLinesEx(mapRow, 1, theme.fieldBorder);
    DrawText(MapDisplayName(settings.editorDefaultMapPath).c_str(), (int)mapRow.x + 6, (int)mapRow.y + 4, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, mapRow) && !anyPopupOpen) {
        mapPicker.Open({mapRow.x, mapRow.y + mapRow.height}, ".starmap");
    }
    fy += 20;
    DrawText("Map loaded automatically when the project is opened in the editor.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;
    std::string pickedMap;
    if (mapPicker.Render(pickedMap)) settings.editorDefaultMapPath = pickedMap;

    DrawText("Game Default Map", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle gameMapRow = {fx, fy, fw, 20};
    DrawRectangleRec(gameMapRow, theme.fieldBg);
    DrawRectangleLinesEx(gameMapRow, 1, theme.fieldBorder);
    DrawText(MapDisplayName(settings.gameDefaultMapPath).c_str(), (int)gameMapRow.x + 6, (int)gameMapRow.y + 4, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, gameMapRow) && !anyPopupOpen) {
        gameMapPicker.Open({gameMapRow.x, gameMapRow.y + gameMapRow.height}, ".starmap");
    }
    fy += 20;
    DrawText("Startup map used by packaged builds of the game.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;
    std::string pickedGameMap;
    if (gameMapPicker.Render(pickedGameMap)) settings.gameDefaultMapPath = pickedGameMap;
}

void ProjectSettingsTab::RenderRenderingCategory(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    Vector2 mp = GetMousePosition();
    const bool anyPopupOpen = rendererPopup.IsOpen() || shadingTypePopup.IsOpen() || moduleSettingsPopupOpen;

    float fx = area.x + 16;
    float fy = area.y + 16;
    float fw = area.width - 32 > 640.0f ? 640.0f : area.width - 32;

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

    DrawText("Render Scale", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle scaleRow = {fx, fy, 100.0f, 20};
    if (!renderScaleChrome) {
        renderScaleChrome = new Widget();
        auto* root = new WidgetElement();
        renderScaleChrome->SetRoot(root);
        renderScaleField = new TextElement();
        renderScaleField->editable = true;
        renderScaleField->numericOnly = true;
        renderScaleField->fontSize = 12.0f;
        renderScaleField->backgroundColor = theme.fieldBg;
        renderScaleField->borderColor = theme.fieldBorder;
        renderScaleField->editingBorderColor = theme.accent;
        renderScaleField->onTextCommitted = [this](const std::string& text) {
            float pct = (float)atof(text.c_str());
            if (pct < 10.0f) pct = 10.0f;
            RenderSettings::Get().SetRenderScale(pct / 100.0f);
        };
        root->AddChild(renderScaleField);
    }
    if (!renderScaleField->isEditing) {
        snprintf(renderScaleBuf, sizeof(renderScaleBuf), "%.2f", RenderSettings::Get().GetRenderScale() * 100.0f);
        renderScaleField->text = renderScaleBuf;
    }
    renderScaleField->size = { scaleRow.width, scaleRow.height };
    renderScaleField->offset = { scaleRow.x, scaleRow.y };
    float dt = GetFrameTime();
    renderScaleChrome->Tick(dt, {0, 0, area.width + area.x, area.height + area.y}, 1.0f);
    WidgetInput::ProcessInput(renderScaleChrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    renderScaleChrome->Draw({0, 0, area.width + area.x, area.height + area.y}, 1.0f);
    DrawText("%", (int)scaleRow.x + 106, (int)scaleRow.y + 4, 12, theme.textSecondary);
    fy += 20;
    DrawText("Percentage of the window resolution the 3D scene renders at, upscaled with nearest-neighbor.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;

    const ShadingTechnique* activeTechnique = ShadingTechniqueRegistry::Get().FindAvailable(currentShadingId);
    moduleSettingsPopupOpen = false;
    if (activeTechnique && activeTechnique->renderSettings) {
        const float sectionPadding = 10.0f;
        float sectionStartY = fy;
        Rectangle sectionBg = {
            fx - sectionPadding, sectionStartY - sectionPadding,
            fw + sectionPadding * 2.0f, moduleSettingsSectionHeight + sectionPadding * 2.0f
        };
        DrawRectangleRec(sectionBg, ColorBrightness(theme.panelBg, -0.12f));

        ModuleSettingsResult result = activeTechnique->renderSettings(fx, fy, fw, activeField, anyPopupOpen);
        fy = result.nextY;
        moduleSettingsPopupOpen = result.popupOpen;
        moduleSettingsSectionHeight = fy - sectionStartY;
        fy += sectionPadding;
    } else {
        moduleSettingsSectionHeight = 0.0f;
    }
}

void ProjectSettingsTab::RenderSaveFilesCategory(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    float fx = area.x + 16;
    float fy = area.y + 16;
    float fw = area.width - 32 > 640.0f ? 640.0f : area.width - 32;

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
        if (InlineBoolBox(encRow, encrypted)) {
            SaveFileRegistry::Get().SetEncrypted(saveFileName, encrypted);
        }
        DrawText("Enc", (int)encRow.x + 22, (int)encRow.y + 2, 10, theme.textSecondary);

        if (InlineButton(delBtn, "x")) removeName = saveFileName;

        fy += 22;
    }
    if (!removeName.empty()) NamedListRegistry::Get().RemoveEntry(SaveFileRegistry::Category(), removeName);

    Rectangle newNameRow = {fx, fy, fw - 60, 20};
    Rectangle addBtn     = {fx + fw - 56, fy, 56, 20};
    if (!newSaveNameChrome) {
        newSaveNameChrome = new Widget();
        auto* root = new WidgetElement();
        newSaveNameChrome->SetRoot(root);
        newSaveNameField = new TextElement();
        newSaveNameField->editable = true;
        newSaveNameField->fontSize = 12.0f;
        newSaveNameField->placeholder = "New save file name";
        newSaveNameField->backgroundColor = theme.fieldBg;
        newSaveNameField->borderColor = theme.fieldBorder;
        newSaveNameField->editingBorderColor = theme.accent;
        newSaveNameField->onTextChanged = [this](const std::string& text) { newSaveFileName = text; };
        root->AddChild(newSaveNameField);
    }
    newSaveNameField->size = { newNameRow.width, newNameRow.height };
    newSaveNameField->offset = { newNameRow.x, newNameRow.y };
    float dt2 = GetFrameTime();
    newSaveNameChrome->Tick(dt2, {0, 0, area.width + area.x, area.height + area.y}, 1.0f);
    WidgetInput::ProcessInput(newSaveNameChrome, WidgetInput::kEditorDevice, dt2, GetMousePosition(), 1.0f);
    newSaveNameChrome->Draw({0, 0, area.width + area.x, area.height + area.y}, 1.0f);
    if (InlineButton(addBtn, "+ Add") && !newSaveFileName.empty()) {
        NamedListRegistry::Get().AddEntry(SaveFileRegistry::Category(), newSaveFileName);
        newSaveFileName.clear();
        newSaveNameField->text.clear();
    }
    fy += 30;

    RenderMigrationSection(area, fx, fy, fw);
}

void ProjectSettingsTab::RebuildLayerFields() {
    const UIPalette& theme = UITheme::Get();
    if (!layerChrome) {
        layerChrome = new Widget();
        layerChrome->SetRoot(new WidgetElement());
    }
    layerChrome->rootElement->ClearChildren();
    layerFields.clear();
    layerFieldIndices = CollisionLayers::UsedIndices();
    for (int index : layerFieldIndices) {
        auto* field = new TextElement();
        field->editable = true;
        field->fontSize = 12.0f;
        field->backgroundColor = theme.fieldBg;
        field->borderColor = theme.fieldBorder;
        field->editingBorderColor = theme.accent;
        field->text = CollisionLayers::NameAt(index);
        field->onTextChanged = [index](const std::string& text) {
            if (CollisionLayers::Rename(index, text)) CollisionLayers::Save();
        };
        layerChrome->rootElement->AddChild(field);
        layerFields.push_back(field);
    }
}

void ProjectSettingsTab::RenderCollisionLayersCategory(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    float fx = area.x + 16;
    float fy = area.y + 16;
    float fw = area.width - 32 > 640.0f ? 640.0f : area.width - 32;

    DrawText("Collision Layers", (int)fx, (int)fy, 14, theme.sectionHeader);
    fy += 24;
    DrawText("A layer is what a collider is. The same names are the channels trace nodes pick from.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 14;
    DrawText("Colliders choose their layer and which layers they collide with. Slot 0 cannot be removed.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 22;

    if (!layerChrome || layerFieldIndices != CollisionLayers::UsedIndices()) RebuildLayerFields();

    int removeIndex = -1;
    for (size_t i = 0; i < layerFieldIndices.size(); i++) {
        int index = layerFieldIndices[i];
        DrawText(TextFormat("%d", index), (int)fx, (int)fy + 4, 12, theme.textSecondary);
        layerFields[i]->size = { fw - 70, 20 };
        layerFields[i]->offset = { fx + 28, fy };
        if (index > 0 && InlineButton({fx + fw - 20, fy, 20, 20}, "x")) removeIndex = index;
        fy += 24;
    }

    float dt = GetFrameTime();
    Rectangle chromeArea = {0, 0, area.width + area.x, area.height + area.y};
    layerChrome->Tick(dt, chromeArea, 1.0f);
    WidgetInput::ProcessInput(layerChrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    layerChrome->Draw(chromeArea, 1.0f);

    if (removeIndex >= 0 && CollisionLayers::Remove(removeIndex)) {
        CollisionLayers::Save();
        RebuildLayerFields();
    }

    fy += 6;
    Rectangle newRow = {fx + 28, fy, fw - 28 - 60, 20};
    Rectangle addBtn = {fx + fw - 56, fy, 56, 20};
    if (!newLayerChrome) {
        newLayerChrome = new Widget();
        auto* root = new WidgetElement();
        newLayerChrome->SetRoot(root);
        newLayerField = new TextElement();
        newLayerField->editable = true;
        newLayerField->fontSize = 12.0f;
        newLayerField->placeholder = "New layer name";
        newLayerField->backgroundColor = theme.fieldBg;
        newLayerField->borderColor = theme.fieldBorder;
        newLayerField->editingBorderColor = theme.accent;
        newLayerField->onTextChanged = [this](const std::string& text) { newLayerName = text; };
        root->AddChild(newLayerField);
    }
    newLayerField->size = { newRow.width, newRow.height };
    newLayerField->offset = { newRow.x, newRow.y };
    newLayerChrome->Tick(dt, chromeArea, 1.0f);
    WidgetInput::ProcessInput(newLayerChrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    newLayerChrome->Draw(chromeArea, 1.0f);
    if (InlineButton(addBtn, "+ Add") && !newLayerName.empty()) {
        if (CollisionLayers::Add(newLayerName) >= 0) {
            CollisionLayers::Save();
            newLayerName.clear();
            newLayerField->text.clear();
            RebuildLayerFields();
        }
    }
    fy += 28;
    if (CollisionLayers::UsedIndices().size() >= (size_t)CollisionLayers::kMaxLayers) {
        DrawText("All 32 layer slots are in use.", (int)fx, (int)fy, 10, theme.accent);
    }
}

static const char* MigrationTypeName(SaveMigration::MigrationOp::Type type) {
    switch (type) {
        case SaveMigration::MigrationOp::Type::RenameActor:    return "Rename Actor";
        case SaveMigration::MigrationOp::Type::RenameVariable: return "Rename Variable";
        case SaveMigration::MigrationOp::Type::RenameKey:      return "Rename Key";
        case SaveMigration::MigrationOp::Type::DeleteKey:      return "Delete Key";
    }
    return "";
}

void ProjectSettingsTab::CommitMigrations() {
    SaveMigration::SetSteps(migrationSteps);
    SaveMigration::SetProjectVersion(migrationVersion);
    SaveMigration::Save();
}

void ProjectSettingsTab::RebuildMigrationFields() {
    const UIPalette& theme = UITheme::Get();
    if (!migrationChrome) {
        migrationChrome = new Widget();
        migrationChrome->SetRoot(new WidgetElement());
    }
    migrationChrome->rootElement->ClearChildren();
    migrationFields.clear();
    if (selectedMigrationStep < 0 || selectedMigrationStep >= (int)migrationSteps.size()) return;

    SaveMigration::MigrationStep& step = migrationSteps[selectedMigrationStep];
    for (size_t i = 0; i < step.ops.size(); i++) {
        using OpType = SaveMigration::MigrationOp::Type;
        OpType type = step.ops[i].type;
        const char* placeholders[3] = {
            "Actor",
            type == OpType::RenameActor ? "Old actor name" : (type == OpType::RenameKey || type == OpType::DeleteKey ? "Key" : "Old variable"),
            type == OpType::RenameActor ? "New actor name" : (type == OpType::RenameKey ? "New key" : "New variable"),
        };
        for (int col = 0; col < 3; col++) {
            auto* field = new TextElement();
            field->editable = true;
            field->fontSize = 12.0f;
            field->placeholder = placeholders[col];
            field->backgroundColor = theme.fieldBg;
            field->borderColor = theme.fieldBorder;
            field->editingBorderColor = theme.accent;
            field->text = col == 0 ? step.ops[i].actor : (col == 1 ? step.ops[i].oldName : step.ops[i].newName);
            int stepIndex = selectedMigrationStep;
            field->onTextChanged = [this, stepIndex, i, col](const std::string& text) {
                if (stepIndex >= (int)migrationSteps.size() || i >= migrationSteps[stepIndex].ops.size()) return;
                SaveMigration::MigrationOp& op = migrationSteps[stepIndex].ops[i];
                if (col == 0) op.actor = text;
                else if (col == 1) op.oldName = text;
                else op.newName = text;
                CommitMigrations();
            };
            migrationChrome->rootElement->AddChild(field);
            migrationFields.push_back(field);
        }
    }
}

void ProjectSettingsTab::RenderMigrationSection(Rectangle area, float fx, float& fy, float fw) {
    const UIPalette& theme = UITheme::Get();
    using OpType = SaveMigration::MigrationOp::Type;

    if (!migrationsLoaded) {
        migrationSteps = SaveMigration::GetSteps();
        migrationVersion = SaveMigration::GetProjectVersion();
        selectedMigrationStep = migrationSteps.empty() ? -1 : (int)migrationSteps.size() - 1;
        migrationsLoaded = true;
        migrationSignature.clear();
    }

    DrawText("Save Data Version", (int)fx, (int)fy, 14, theme.sectionHeader);
    fy += 24;
    DrawText("Saves older than the current version are migrated when loaded, then rewritten. Keys are actor and variable name based.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 18;

    DrawText(TextFormat("Current version: %d", migrationVersion), (int)fx, (int)fy + 4, 12, theme.textPrimary);
    if (InlineButton({fx + 150, fy, 100, 20}, "+ Add Step")) {
        SaveMigration::MigrationStep step;
        step.fromVersion = migrationVersion;
        step.toVersion = migrationVersion + 1;
        migrationSteps.push_back(step);
        migrationVersion = step.toVersion;
        selectedMigrationStep = (int)migrationSteps.size() - 1;
        CommitMigrations();
    }
    if (!migrationSteps.empty() && InlineButton({fx + 256, fy, 100, 20}, "Remove Last")) {
        migrationVersion = migrationSteps.back().fromVersion;
        migrationSteps.pop_back();
        if (selectedMigrationStep >= (int)migrationSteps.size()) selectedMigrationStep = (int)migrationSteps.size() - 1;
        CommitMigrations();
    }
    fy += 28;

    std::string warning;
    int expectedFrom = 0;
    for (const SaveMigration::MigrationStep& step : migrationSteps) {
        if (step.fromVersion != expectedFrom) { warning = "Steps do not form a chain; saves may skip a step."; break; }
        expectedFrom = step.toVersion;
    }
    if (warning.empty() && !migrationSteps.empty() && expectedFrom != migrationVersion) warning = "Last step does not reach the current version.";
    if (!warning.empty()) {
        DrawText(warning.c_str(), (int)fx, (int)fy, 10, theme.accent);
        fy += 16;
    }

    float stepX = fx;
    for (int i = 0; i < (int)migrationSteps.size(); i++) {
        Rectangle r = {stepX, fy, 64, 20};
        if (i == selectedMigrationStep) DrawRectangleRec({r.x - 2, r.y - 2, r.width + 4, r.height + 4}, theme.accent);
        std::string label = std::to_string(migrationSteps[i].fromVersion) + " > " + std::to_string(migrationSteps[i].toVersion);
        if (InlineButton(r, label.c_str())) selectedMigrationStep = i;
        stepX += 70;
        if (stepX + 64 > fx + fw) break;
    }
    fy += 30;

    if (selectedMigrationStep < 0 || selectedMigrationStep >= (int)migrationSteps.size()) {
        DrawText("No migration steps. Add a step when a rename would orphan existing saves.", (int)fx, (int)fy, 10, theme.textSecondary);
        fy += 20;
        return;
    }

    SaveMigration::MigrationStep& step = migrationSteps[selectedMigrationStep];
    std::string signature = std::to_string(selectedMigrationStep);
    for (const SaveMigration::MigrationOp& op : step.ops) signature += "," + std::to_string((int)op.type);
    if (signature != migrationSignature || !migrationChrome) {
        migrationSignature = signature;
        RebuildMigrationFields();
    }

    float typeW = 120.0f;
    float delW = 20.0f;
    float colW = (fw - typeW - delW - 4 * 6.0f) / 3.0f;
    int removeIndex = -1;
    int cycleIndex = -1;
    for (size_t i = 0; i < step.ops.size(); i++) {
        float x = fx;
        if (InlineButton({x, fy, typeW, 20}, MigrationTypeName(step.ops[i].type))) cycleIndex = (int)i;
        x += typeW + 6;
        bool usesActor = step.ops[i].type != OpType::RenameActor;
        for (int col = 0; col < 3; col++) {
            TextElement* field = (i * 3 + col < migrationFields.size()) ? migrationFields[i * 3 + col] : nullptr;
            bool used = col != 0 || usesActor;
            bool usesNew = col != 2 || step.ops[i].type != OpType::DeleteKey;
            if (field) {
                field->visible = used && usesNew;
                field->size = { colW, 20 };
                field->offset = { x, fy };
            }
            x += colW + 6;
        }
        if (InlineButton({x, fy, delW, 20}, "x")) removeIndex = (int)i;
        fy += 24;
    }

    float dt = GetFrameTime();
    Rectangle chromeArea = {0, 0, area.width + area.x, area.height + area.y};
    migrationChrome->Tick(dt, chromeArea, 1.0f);
    WidgetInput::ProcessInput(migrationChrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    migrationChrome->Draw(chromeArea, 1.0f);

    if (InlineButton({fx, fy, 70, 20}, "+ Op")) {
        SaveMigration::MigrationOp op;
        op.type = OpType::RenameVariable;
        step.ops.push_back(op);
        CommitMigrations();
    }
    fy += 28;

    if (cycleIndex >= 0) {
        step.ops[cycleIndex].type = (OpType)(((int)step.ops[cycleIndex].type + 1) % 4);
        CommitMigrations();
    }
    if (removeIndex >= 0) {
        step.ops.erase(step.ops.begin() + removeIndex);
        CommitMigrations();
    }
}

void ProjectSettingsTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { "Save Project" } },
        { "Edit",   { } },
        { "Window", { } },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}
