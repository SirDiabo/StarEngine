#include "AssetPickerPopup.h"
#include "Core/Project.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

std::vector<std::function<void()>> AssetPickerPopup::deferredDraws;

void AssetPickerPopup::Open(Vector2 anchorPos, const std::string& ext) {
    OpenWithExtras(anchorPos, ext, {});
}

void AssetPickerPopup::OpenWithExtras(Vector2 anchorPos, const std::string& ext,
                                       const std::vector<std::pair<std::string, std::string>>& extras) {
    open = true;
    justOpened = true;
    extension = ext;
    displayNames.clear();
    contentPaths.clear();

    for (const auto& extra : extras) {
        displayNames.push_back(extra.first);
        contentPaths.push_back(extra.second);
    }

    fs::path contentRoot = fs::path(Project::Get().GetRootPath()) / "Content";
    if (fs::exists(contentRoot)) {
        for (const auto& entry : fs::recursive_directory_iterator(contentRoot)) {
            if (entry.is_directory()) continue;
            if (entry.path().extension().string() != extension) continue;

            fs::path relative = fs::relative(entry.path(), contentRoot);
            displayNames.push_back(relative.stem().string());
            contentPaths.push_back(relative.generic_string());
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
        UIWidget::Panel(areaCopy);
    });

    float y = area.y + 2;

    Rectangle noneItem = {area.x + 1, y, area.width - 2, 18};
    bool noneHovered = CheckCollisionPointRec(mousePos, noneItem);
    deferredDraws.push_back([noneItem, noneHovered]() {
        const UIPalette& theme = UITheme::Get();
        if (noneHovered) DrawRectangleRec(noneItem, theme.surfaceHover);
        DrawText("None", (int)noneItem.x + 6, (int)noneItem.y + 3, 11, theme.textSecondary);
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
            const UIPalette& theme = UITheme::Get();
            if (hovered) DrawRectangleRec(item, theme.surfaceHover);
            DrawText(name.c_str(), (int)item.x + 6, (int)item.y + 3, 11, theme.textPrimary);
        });

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            outPath = contentPaths[i];
            Close();
            return true;
        }
        y += 18.0f;
    }

    if (displayNames.empty()) {
        float emptyY = y;
        float emptyX = area.x;
        deferredDraws.push_back([emptyX, emptyY]() {
            DrawText("(no matching assets)", (int)emptyX + 6, (int)emptyY, 10, UITheme::Get().textSecondary);
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