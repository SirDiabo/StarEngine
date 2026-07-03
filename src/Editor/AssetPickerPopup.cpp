#include "AssetPickerPopup.h"
#include "Core/Project.h"
#include "raygui.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

std::vector<std::function<void()>> AssetPickerPopup::deferredDraws;

void AssetPickerPopup::Open(Vector2 anchorPos, const std::string& ext) {
    open = true;
    justOpened = true;
    extension = ext;
    displayNames.clear();
    fullPaths.clear();

    fs::path contentRoot = fs::path(Project::Get().GetRootPath()) / "Content";
    if (fs::exists(contentRoot)) {
        for (const auto& entry : fs::recursive_directory_iterator(contentRoot)) {
            if (entry.is_directory()) continue;
            if (entry.path().extension().string() != extension) continue;

            fs::path relative = fs::relative(entry.path(), contentRoot);
            displayNames.push_back(relative.stem().string());
            fullPaths.push_back(entry.path().string());
        }
    }

    float height = 22.0f + (float)(displayNames.size() + 1) * 18.0f + 4.0f;
    area = {anchorPos.x, anchorPos.y, 200.0f, height};
}

void AssetPickerPopup::Close() {
    open = false;
}

bool AssetPickerPopup::Render(std::string& outPath) {
    if (!open) return false;

    Vector2 mousePos = GetMousePosition();

    Rectangle areaCopy = area;
    deferredDraws.push_back([areaCopy]() {
        DrawRectangleRec(areaCopy, {45, 45, 50, 255});
        DrawRectangleLinesEx(areaCopy, 1, {80, 80, 88, 255});
    });

    float y = area.y + 2;

    Rectangle noneItem = {area.x + 1, y, area.width - 2, 18};
    bool noneHovered = CheckCollisionPointRec(mousePos, noneItem);
    deferredDraws.push_back([noneItem, noneHovered]() {
        if (noneHovered) DrawRectangleRec(noneItem, {70, 70, 80, 255});
        DrawText("None", (int)noneItem.x + 6, (int)noneItem.y + 3, 11, {180, 180, 190, 255});
    });
    if (noneHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        outPath.clear();
        Close();
        return true;
    }
    y += 18.0f;

    for (size_t i = 0; i < displayNames.size(); i++) {
        Rectangle item = {area.x + 1, y, area.width - 2, 18};
        bool hovered = CheckCollisionPointRec(mousePos, item);
        std::string name = displayNames[i];
        deferredDraws.push_back([item, hovered, name]() {
            if (hovered) DrawRectangleRec(item, {70, 70, 80, 255});
            DrawText(name.c_str(), (int)item.x + 6, (int)item.y + 3, 11, WHITE);
        });

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            outPath = fullPaths[i];
            Close();
            return true;
        }
        y += 18.0f;
    }

    if (displayNames.empty()) {
        float emptyY = y;
        float emptyX = area.x;
        deferredDraws.push_back([emptyX, emptyY]() {
            DrawText("(no matching assets)", (int)emptyX + 6, (int)emptyY, 10, {110, 110, 118, 255});
        });
    }

    if (justOpened) {
        justOpened = false;
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, area)) {
        Close();
    }

    return false;
}

void AssetPickerPopup::FlushDeferredDraws() {
    for (const auto& draw : deferredDraws) draw();
    deferredDraws.clear();
}