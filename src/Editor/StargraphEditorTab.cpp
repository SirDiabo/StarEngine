#include "StargraphEditorTab.h"
#include "StargraphEditorPanel.h"
#include "MenuBar.h"
#include <filesystem>

namespace fs = std::filesystem;

StargraphEditorTab::StargraphEditorTab(const std::string& path)
    : assetPath(path) {
    title = fs::path(path).stem().string();
    permanent = false;
}

StargraphEditorTab::~StargraphEditorTab() {
    delete panel;
}

void StargraphEditorTab::Initialize() {
    panel = new StargraphEditorPanel();
    panel->Initialize();
    panel->Open(assetPath);
}

void StargraphEditorTab::Render(float x, float y, float width, float height) {
    panel->bounds = { x, y, width, height };
    panel->Render();
}

void StargraphEditorTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { "Save", "Save All" } },
        { "Asset",  { "Find in Content Browser" } },
        { "Debug",  { "Play Graph", "Stop" } },
        { "Window", { } },
        { "Help",   { } },
    });
}

bool StargraphEditorTab::IsOpen() const {
    return panel && panel->IsOpen();
}