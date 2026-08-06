#include "SearchableListPopup.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"
#include <algorithm>
#include <cctype>

std::vector<std::function<void()>> SearchableListPopup::deferredDraws;

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
    if (popup.x < boundsArea.x) popup.x = boundsArea.x;
    if (popup.y < boundsArea.y) popup.y = boundsArea.y;
    lastRect = popup;

    Vector2 mp = GetMousePosition();

    bool wasJustOpened = justOpened;
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

    Rectangle popupCopy = popup;
    deferredDraws.push_back([popupCopy]() {
        DrawRectangleRec(popupCopy, UITheme::Get().panelBg);
        DrawRectangleLinesEx(popupCopy, 1, UITheme::Get().panelBorder);
    });

    Rectangle searchRect = {popup.x + 4, popup.y + 4, popup.width - 8, searchH - 6};
    char* searchBuf = search;
    size_t searchBufSize = sizeof(search);
    deferredDraws.push_back([searchRect, searchBuf, searchBufSize]() {
        GuiTextBox(searchRect, searchBuf, searchBufSize, true);
    });

    Rectangle listArea = {popup.x, popup.y + searchH, popup.width, listH};
    if (CheckCollisionPointRec(mp, listArea)) {
        float wheel = GetMouseWheelMove();
        scrollOffset -= wheel * itemH * 2;
    }
    float maxScroll = totalItemsH - listH;
    if (maxScroll < 0) maxScroll = 0;
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;

    struct DrawItem {
        bool isHeader;
        std::string text;
        Rectangle rect;
        bool hovered;
        bool drawDivider;
    };
    std::vector<DrawItem> drawItems;

    std::string clickedId;
    std::string lastCategory;
    float y = popup.y + searchH + 4 - scrollOffset;
    for (const Entry& e : filtered) {
        if (e.category != lastCategory) {
            drawItems.push_back({true, e.category, {popup.x, y, popup.width, categoryHeaderH}, false, !lastCategory.empty()});
            lastCategory = e.category;
            y += categoryHeaderH;
        }

        Rectangle btn = {popup.x + 4, y, popup.width - 8, itemH - 2};
        bool hovered = CheckCollisionPointRec(mp, btn) && CheckCollisionPointRec(mp, listArea);
        if (hovered && !wasJustOpened && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clickedId = e.id;
        drawItems.push_back({false, e.label, btn, hovered, false});
        y += itemH;
    }

    Rectangle listAreaCopy = listArea;
    Rectangle popupCopy2 = popup;
    float totalItemsHCopy = totalItemsH;
    float listHCopy = listH;
    float scrollOffsetCopy = scrollOffset;
    float maxScrollCopy = maxScroll;
    deferredDraws.push_back([drawItems, listAreaCopy, popupCopy2, totalItemsHCopy, listHCopy, scrollOffsetCopy, maxScrollCopy]() {
        const UIPalette& theme = UITheme::Get();
        BeginScissorMode((int)listAreaCopy.x, (int)listAreaCopy.y, (int)listAreaCopy.width, (int)listAreaCopy.height);
        for (const auto& item : drawItems) {
            if (item.isHeader) {
                if (item.drawDivider) {
                    DrawLineV({item.rect.x + 6, item.rect.y}, {item.rect.x + item.rect.width - 6, item.rect.y}, theme.panelBorder);
                }
                DrawText(item.text.c_str(), (int)item.rect.x + 8, (int)item.rect.y + 4, 11, theme.accent);
            } else {
                if (item.hovered) DrawRectangleRec(item.rect, theme.surfaceHover);
                DrawText(item.text.c_str(), (int)item.rect.x + 8, (int)item.rect.y + 4, 12, theme.textPrimary);
            }
        }
        EndScissorMode();

        if (totalItemsHCopy > listHCopy) {
            float scrollRatio = (maxScrollCopy > 0) ? scrollOffsetCopy / maxScrollCopy : 0;
            float barH = (listHCopy / totalItemsHCopy) * listHCopy;
            float barY = listAreaCopy.y + scrollRatio * (listHCopy - barH);
            DrawRectangleRec({popupCopy2.x + popupCopy2.width - 4, barY, 3, barH}, {100, 100, 120, 200});
        }
    });

    if (!clickedId.empty()) open = false;
    return clickedId;
}

void SearchableListPopup::FlushDeferredDraws() {
    for (const auto& draw : deferredDraws) draw();
    deferredDraws.clear();
}
