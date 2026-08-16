#include "StargraphPanel.h"
#include "StargraphEditorPanel.h"

void StargraphPanel::Initialize() {
    name = "My Stargraph";
}

void StargraphPanel::Render() {
    if (session) session->RenderLeftPanel(bounds);
}
