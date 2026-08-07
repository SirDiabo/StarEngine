#include "AnimationPreviewTab.h"
#include "MenuBar.h"
#include "Core/StarAsset.h"
#include "Core/Project.h"
#include "Runtime/AssetCache.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raylib.h"
#include <filesystem>
#include <cmath>
#include <cstdio>

namespace fs = std::filesystem;

static const float TOP_H = 28.0f;
static const float BOTTOM_H = 56.0f;
static const float RIGHT_W = 260.0f;
static const Color TOP_COL = {36, 36, 40, 255};
static const Color LINE_COL = {60, 60, 65, 255};

AnimationPreviewTab::AnimationPreviewTab(const std::string& path) : animPath(path) {
    title = fs::path(path).stem().string();
    permanent = false;
}

void AnimationPreviewTab::Initialize() {
    StarAsset asset;
    if (StarAsset::LoadFromFile(animPath, asset) && asset.assetType == "Animation") {
        animName = asset.animName;
        frameCount = asset.animFrameCount;
        animBoneCount = asset.animBoneCount;
        fps = asset.animFramesPerSecond > 0.0f ? asset.animFramesPerSecond : 30.0f;
    }
    viewport.Initialize();
    RefreshCompatibleMeshes();
}

void AnimationPreviewTab::RefreshCompatibleMeshes() {
    compatibleMeshPaths.clear();
    std::string contentRoot = Project::Get().GetRootPath() + "/Content";
    std::error_code ec;
    if (!fs::exists(contentRoot, ec)) return;

    for (const auto& entry : fs::recursive_directory_iterator(
             contentRoot, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        if (entry.path().extension() != ".starasset") continue;
        StarAsset meshAsset;
        if (!StarAsset::LoadFromFile(entry.path().string(), meshAsset)) continue;
        if (meshAsset.assetType != "StaticMesh" || !meshAsset.hasSkeleton) continue;
        if (animBoneCount > 0 && meshAsset.skeleton.size() != animBoneCount) continue;
        compatibleMeshPaths.push_back(entry.path().string());
    }

    if (meshPath.empty() && !compatibleMeshPaths.empty()) {
        meshPath = compatibleMeshPaths[0];
        viewport.SetAsset(meshPath);
    }
}

void AnimationPreviewTab::Render(float x, float y, float width, float height) {
    const UIPalette& theme = UITheme::Get();

    Rectangle top = {x, y, width, TOP_H};
    DrawRectangleRec(top, TOP_COL);
    DrawLine((int)top.x, (int)(top.y + top.height), (int)(top.x + top.width),
             (int)(top.y + top.height), LINE_COL);
    DrawText(title.c_str(), (int)x + 6, (int)y + 8, 13, WHITE);

    float contentY = y + TOP_H;
    float contentH = height - TOP_H - BOTTOM_H;
    float viewW = width - RIGHT_W;
    if (viewW < 100.0f) viewW = 100.0f;
    if (contentH < 0.0f) contentH = 0.0f;

    float duration = (fps > 0.0f) ? (float)frameCount / fps : 0.0f;

    if (playing && duration > 0.0f) {
        timeSeconds += GetFrameTime();
        timeSeconds = fmodf(timeSeconds, duration);
        if (timeSeconds < 0.0f) timeSeconds += duration;
    }

    ModelAnimation* anim = AssetCache::Get().GetOrLoadAnimation(animPath);
    if (anim != nullptr) viewport.SetPreviewAnimation(anim, timeSeconds * fps);
    else viewport.SetPreviewAnimation(nullptr, 0.0f);

    viewport.Render(x, contentY, viewW, contentH);

    Rectangle rightPanel = {x + viewW, contentY, RIGHT_W, contentH};
    DrawRectangleRec(rightPanel, theme.panelBg);
    DrawRectangleLinesEx(rightPanel, 1, theme.panelBorder);

    float ry = rightPanel.y + 10;
    DrawText("Animation", (int)rightPanel.x + 10, (int)ry, 12, theme.textSecondary);
    ry += 18;
    std::string nameLabel = animName.empty() ? "(unnamed)" : animName;
    DrawText(nameLabel.c_str(), (int)rightPanel.x + 10, (int)ry, 13, theme.textPrimary);
    ry += 20;
    char infoBuf[64];
    snprintf(infoBuf, sizeof(infoBuf), "%d frames @ %d fps", frameCount, (int)fps);
    DrawText(infoBuf, (int)rightPanel.x + 10, (int)ry, 11, theme.textSecondary);
    ry += 26;

    DrawText("Preview Mesh", (int)rightPanel.x + 10, (int)ry, 12, theme.textSecondary);
    ry += 16;
    Rectangle meshBtn = {rightPanel.x + 10, ry, rightPanel.width - 20, 22};
    std::string meshLabel = meshPath.empty() ? "(none)" : fs::path(meshPath).stem().string();
    if (UIWidget::Button(meshBtn, meshLabel.c_str())) {
        if (meshPicker.IsOpen()) meshPicker.Close();
        else meshPicker.Open({meshBtn.x, meshBtn.y + meshBtn.height});
    }
    ry += 30;

    if (compatibleMeshPaths.empty()) {
        DrawText("No compatible skeletal", (int)rightPanel.x + 10, (int)ry, 10, theme.danger);
        ry += 14;
        DrawText("meshes found in Content.", (int)rightPanel.x + 10, (int)ry, 10, theme.danger);
        ry += 18;
    }

    if (meshPicker.IsOpen()) {
        std::vector<SearchableListPopup::Entry> entries;
        for (const std::string& p : compatibleMeshPaths) {
            entries.push_back({"Meshes", p, fs::path(p).stem().string()});
        }
        std::string picked = meshPicker.Render(entries, {x, y, width, height});
        if (!picked.empty()) {
            meshPath = picked;
            viewport.SetAsset(meshPath);
        }
    }

    Rectangle bottom = {x, y + height - BOTTOM_H, width, BOTTOM_H};
    DrawRectangleRec(bottom, TOP_COL);
    DrawLine((int)bottom.x, (int)bottom.y, (int)(bottom.x + bottom.width), (int)bottom.y, LINE_COL);

    Rectangle playBtn = {bottom.x + 10, bottom.y + 8, 70, 24};
    if (UIWidget::Button(playBtn, playing ? "Pause" : "Play")) playing = !playing;

    Rectangle stopBtn = {bottom.x + 88, bottom.y + 8, 60, 24};
    if (UIWidget::Button(stopBtn, "Stop")) { playing = false; timeSeconds = 0.0f; }

    float scrubX = bottom.x + 158;
    float scrubW = bottom.width - 168;
    if (scrubW < 40.0f) scrubW = 40.0f;
    Rectangle scrubRect = {scrubX, bottom.y + 12, scrubW, 12};
    DrawRectangleRec(scrubRect, {20, 20, 24, 255});
    if (duration > 0.0f) {
        float t = timeSeconds / duration;
        DrawRectangleRec({scrubRect.x, scrubRect.y, scrubRect.width * t, scrubRect.height}, theme.accent);
    }
    DrawRectangleLinesEx(scrubRect, 1, theme.panelBorder);

    Vector2 mp = GetMousePosition();
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, scrubRect) && duration > 0.0f) {
        float t = (mp.x - scrubRect.x) / scrubRect.width;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        timeSeconds = t * duration;
        playing = false;
    }

    char timeLabel[32];
    snprintf(timeLabel, sizeof(timeLabel), "%.2fs / %.2fs", timeSeconds, duration);
    DrawText(timeLabel, (int)scrubRect.x, (int)(scrubRect.y + 18), 11, theme.textSecondary);
}

void AnimationPreviewTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { } },
        { "Asset",  { "Find in Content Browser" } },
        { "Window", { } },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}
