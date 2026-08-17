#include "StargraphOutlinerPanel.h"
#include "StargraphEditorPanel.h"

void StargraphOutlinerPanel::Initialize() {
    name = "Components";
}

void StargraphOutlinerPanel::Render() {
    if (session) session->RenderComponentOutliner(bounds);
}
