#include "StargraphGraphPanel.h"
#include "StargraphEditorPanel.h"

void StargraphGraphPanel::Initialize() {
    name = "Graph";
}

void StargraphGraphPanel::Render() {
    if (!session) return;
    session->RenderGraph(bounds);
    session->RenderNodeContextMenu();
}
