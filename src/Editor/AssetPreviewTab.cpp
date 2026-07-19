#include "AssetPreviewTab.h"
#include "MenuBar.h"
#include "AssetImporter.h"
#include "Core/StarAsset.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>
#include <filesystem>

namespace fs = std::filesystem;

static const float PREVIEW_RIGHT_W  = 300.0f;
static const float PREVIEW_TOP_H    = 28.0f;
static const Color PREVIEW_TOP_COL  = {36, 36, 40, 255};
static const Color PREVIEW_LINE_COL = {60, 60, 65, 255};

AssetPreviewTab::AssetPreviewTab(const std::string& path)
    : assetPath(path) {
    title = fs::path(path).stem().string();
    permanent = false;
}

AssetPreviewTab::~AssetPreviewTab() {
    ImportDetailsPanel::Get().Close();
}

void AssetPreviewTab::Initialize() {
    StarAsset asset;
    StarAsset::LoadFromFile(assetPath, asset);
    assetType = asset.assetType;

    viewport.Initialize();
    viewport.SetAsset(assetPath);

    ImportDetailsPanel::Get().Open(assetPath, assetType, true);
}

void AssetPreviewTab::Render(float x, float y, float width, float height) {
    Rectangle top = {x, y, width, PREVIEW_TOP_H};
    DrawRectangleRec(top, PREVIEW_TOP_COL);
    DrawLine((int)top.x, (int)(top.y + top.height), (int)(top.x + top.width),
             (int)(top.y + top.height), PREVIEW_LINE_COL);

    DrawText(title.c_str(), (int)x + 6, (int)y + 8, 13, WHITE);

    float reimportW = 70.0f;
    Rectangle reimportBtn = {x + width - reimportW - 4, y + 3, reimportW, PREVIEW_TOP_H - 6};
    if (GuiButton(reimportBtn, "Reimport")) {
        std::string reimported = AssetImporter::Reimport(assetPath);
        if (!reimported.empty()) {
            viewport.OnReimport();
    ImportDetailsPanel::Get().Open(assetPath, assetType, true);
        }
    }

    float contentY = y + PREVIEW_TOP_H;
    float contentH = height - PREVIEW_TOP_H;

    float viewW = width - PREVIEW_RIGHT_W;
    if (viewW < 100.0f) viewW = 100.0f;
    viewport.Render(x, contentY, viewW, contentH);

    Rectangle rightPanel = {x + viewW, contentY, PREVIEW_RIGHT_W, contentH};
    ImportDetailsPanel::Get().RenderInline(rightPanel);
}

void AssetPreviewTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { "Save", "Save All" } },
        { "Asset",  { "Find in Content Browser" } },
        { "Window", { } },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}
