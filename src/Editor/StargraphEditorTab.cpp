#include "StargraphEditorTab.h"
#include "StargraphEditorPanel.h"
#include "LogPanel.h"
#include "MenuBar.h"
#include "raylib.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

static const float LOG_MIN_H      = 60.0f;
static const float SPLITTER_H     = 4.0f;
static const Color SPLITTER_COL   = { 28, 28, 30, 255 };
static const Color SPLITTER_HOV   = { 80, 120, 200, 255 };

StargraphEditorTab::StargraphEditorTab(const std::string& path)
    : assetPath(path) {
    title     = fs::path(path).stem().string();
    permanent = false;
}

StargraphEditorTab::~StargraphEditorTab() {
    delete panel;
    // logPanel is owned by the Level Editor tab and shared across all editor tabs; not deleted here.
}

void StargraphEditorTab::Initialize() {
    panel = new StargraphEditorPanel();
    panel->Initialize();
    panel->Open(assetPath);
}

void StargraphEditorTab::Render(float x, float y, float width, float height) {
    // Vertical split: graph editor on top, log panel on bottom.
    float graphH    = (height - SPLITTER_H) * logSplitRatio;
    float logH      = height - graphH - SPLITTER_H;
    float splitterY = y + graphH;

    // Splitter interaction.
    Rectangle splitter = { x, splitterY, width, SPLITTER_H };
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, splitter);

    DrawRectangleRec(splitter, (hovered || resizingSplitter) ? SPLITTER_HOV : SPLITTER_COL);

    if ((hovered || resizingSplitter) && !resizingSplitter)
        SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
    else if (!resizingSplitter)
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);

    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        resizingSplitter  = true;
        splitterDragStart = mouse.y;
        ratioAtDragStart  = logSplitRatio;
    }
    if (resizingSplitter) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
            float delta    = mouse.y - splitterDragStart;
            float newRatio = ratioAtDragStart + delta / height;
            float minRatio = LOG_MIN_H / height;
            logSplitRatio  = std::max(minRatio, std::min(1.0f - minRatio, newRatio));
        } else {
            resizingSplitter = false;
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }
    }

    panel->bounds = { x, y, width, graphH };
    panel->Render();

    if (logPanel) {
        logPanel->bounds = { x, splitterY + SPLITTER_H, width, logH };
        logPanel->Render();
    }
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