#include "Editor.h"
#include "ViewportPanel.h"
#include "DetailsPanel.h"
#include "ContentBrowserPanel.h"

void Editor::Initialize() {
    panels.push_back(new ViewportPanel());
    panels.push_back(new DetailsPanel());
    panels.push_back(new ContentBrowserPanel());

    for (auto* panel : panels) {
        panel->Initialize();
    }
}

void Editor::Render() {
    for (auto* panel : panels) {
        if (panel->IsVisible()) {
            panel->Render();
        }
    }
}