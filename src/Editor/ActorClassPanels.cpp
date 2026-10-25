#include "ActorClassPanels.h"
#include "ActorClassEditor.h"

void ComponentOutlinerPanel::Initialize() {
    name = "Components";
}

void ComponentOutlinerPanel::Render() {
    if (!editor) return;
    Rectangle screen = screenBounds ? screenBounds() : Rectangle{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
    editor->RenderOutliner(bounds, screen);
}

void ActorPreviewPanel::Initialize() {
    name = "Preview";
}

void ActorPreviewPanel::Render() {
    if (editor) editor->RenderPreview(bounds);
}

void ComponentDetailsPanel::SetEditor(ActorClassEditor* e) {
    editor = e;
    buildRows = [this]() -> std::vector<PropertyRow> {
        std::vector<PropertyRow> rows;
        if (!editor) return rows;

        editor->ClampSelection();
        std::string key = editor->DetailsSelectionKey();
        if (key != lastSelectionKey) {
            editor->ResetPickers();
            lastSelectionKey = key;
        }

        editor->BuildDetailsRows(rows);
        for (PropertyRow& row : rows) row.id = key + "|" + row.id;
        return rows;
    };
    onAssetPicked = [this](const std::string& fullRowId, const std::string& pickedPath) {
        if (!editor) return;
        size_t separator = fullRowId.find('|');
        const std::string rowId = (separator == std::string::npos) ? fullRowId : fullRowId.substr(separator + 1);
        editor->OnAssetPicked(rowId, pickedPath);
    };
    onRenderExtras = [this]() {
        if (editor) editor->RenderPickers(bounds);
    };
}
