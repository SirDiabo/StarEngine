#include "StargraphPreviewPanel.h"
#include "StargraphEditorPanel.h"

void StargraphPreviewPanel::Initialize() {
    name = "Preview";
}

void StargraphPreviewPanel::Render() {
    if (session) session->RenderPreviewViewport(bounds);
}
