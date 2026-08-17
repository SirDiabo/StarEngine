#include "StargraphDetailsPanel.h"
#include "StargraphEditorPanel.h"

void StargraphDetailsPanel::Initialize() {
    name = "Details";
}

void StargraphDetailsPanel::Render() {
    if (session) session->RenderRightPanel(bounds);
}
