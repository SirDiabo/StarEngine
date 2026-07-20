#include "SearchableListPopup.h"
#include "raygui.h"
#include <algorithm>
#include <cctype>

void SearchableListPopup::Open(Vector2 anchorPos, float widthIn, float maxVisibleHeightIn) {
    open = true;
    justOpened = true;
    pos = anchorPos;
    width = widthIn;
    maxVisibleHeight = maxVisibleHeightIn;
    scrollOffset = 0.0f;
    search[0] = '\0';
}

void SearchableListPopup::Close() {
    open = false;
}

std::string SearchableListPopup::Render(const std::vector<Entry>& entries, Rectangle boundsArea) {
    if (!open) return "";

    std::string searchLower = search;
    for (char& c : searchLower) c = (char)tolower(c);

    std::vector<Entry> filtered;
    for (const Entry& e : entries) {
        if (!searchLower.empty()) {
            std::string labelLower = e.label;
            for (char& c : labelLower) c = (char)tolower(c);
            if (labelLower.find(searchLower) == std::string::npos) continue;
        }
        filtered.push_back(e);
    }
    std::sort(filtered.begin(), filtered.end(), [](const Entry& a, const Entry& b) {
        if (a.category != b.category) return a.category < b.category;
        return a.label < b.label;
    });

    if (filtered.empty() && searchLower.empty() && entries.empty()) { open = false; return ""; }

    float itemH = 22;
    float searchH = 26;
    float categoryHeaderH = 18;

    int categoryCount = 0;
    {
        std::string prev;
        for (const Entry& e : filtered) {
            if (e.category != prev) { categoryCount++; prev = e.category; }
        }
    }

    float totalItemsH = (float)filtered.size() * itemH + (float)categoryCount * categoryHeaderH + 10;
    float listH = (totalItemsH < maxVisibleHeight) ? totalItemsH : maxVisibleHeight;
    float menuH = searchH + listH;

    Rectangle popup = {pos.x, pos.y, width, menuH};
    if (popup.x + popup.width  > boundsArea.x + boundsArea.width)  popup.x = boundsArea.x + boundsArea.width  - popup.width;
    if (popup.y + popup.height > boundsArea.y + boundsArea.height) popup.y = boundsArea.y + boundsArea.height - popup.height;
    lastRect = popup;

    Vector2 mp = GetMousePosition();

    if (justOpened) {
        justOpened = false;
    } else {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, popup)) {
            open = false;
            return "";
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            open = false;
            return "";
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        open = false;
        return "";
    }

    DrawRectangleRec(popup, {44, 44, 50, 255});
    DrawRectangleLinesEx(popup, 1, {80, 80, 90, 255});

    Rectangle searchRect = {popup.x + 4, popup.y + 4, popup.width - 8, searchH - 6};
    GuiTextBox(searchRect, search, sizeof(search), true);

    Rectangle listArea = {popup.x, popup.y + searchH, popup.width, listH};
    if (CheckCollisionPointRec(mp, listArea)) {
        float wheel = GetMouseWheelMove();
        scrollOffset -= wheel * itemH * 2;
    }
    float maxScroll = totalItemsH - listH;
    if (maxScroll < 0) maxScroll = 0;
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;

    BeginScissorMode((int)listArea.x, (int)listArea.y, (int)listArea.width, (int)listArea.height);

    std::string clickedId;
    std::string lastCategory;
    float y = popup.y + searchH + 4 - scrollOffset;
    for (const Entry& e : filtered) {
        if (e.category != lastCategory) {
            if (!lastCategory.empty()) {
                DrawLineV({popup.x + 6, y}, {popup.x + popup.width - 6, y}, {70, 70, 80, 255});
            }
            lastCategory = e.category;
            DrawText(e.category.c_str(), (int)popup.x + 8, (int)y + 4, 11, {150, 165, 195, 255});
            y += categoryHeaderH;
        }

        Rectangle btn = {popup.x + 4, y, popup.width - 8, itemH - 2};
        if (CheckCollisionPointRec(mp, btn) && CheckCollisionPointRec(mp, listArea)) {
            DrawRectangleRec(btn, {65, 65, 85, 255});
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clickedId = e.id;
        }
        DrawText(e.label.c_str(), (int)btn.x + 8, (int)btn.y + 4, 12, WHITE);
        y += itemH;
    }

    EndScissorMode();

    if (totalItemsH > listH) {
        float scrollRatio = (maxScroll > 0) ? scrollOffset / maxScroll : 0;
        float barH = (listH / totalItemsH) * listH;
        float barY = listArea.y + scrollRatio * (listH - barH);
        DrawRectangleRec({popup.x + popup.width - 4, barY, 3, barH}, {100, 100, 120, 200});
    }

    if (!clickedId.empty()) open = false;
    return clickedId;
}
