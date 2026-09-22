#include "StargraphDetailsPanel.h"
#include "StargraphEditorPanel.h"

void StargraphDetailsPanel::SetSession(StargraphEditorPanel* s) {
    session = s;
    buildRows = [this]() -> std::vector<PropertyRow> {
        return session ? session->BuildDetailsRows() : std::vector<PropertyRow>();
    };
    onAssetPicked = [this](const std::string& rowId, const std::string& pickedPath) {
        if (session) session->OnDetailAssetPicked(rowId, pickedPath);
    };
    onRenderExtras = [this]() {
        if (session) session->RenderDetailPickers(bounds);
    };
}
