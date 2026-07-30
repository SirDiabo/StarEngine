#include "RegistryPickerPopup.h"
#include "Core/NamedListRegistry.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"

std::vector<std::function<void()>> RegistryPickerPopup::deferredDraws;

void RegistryPickerPopup::Open(Vector2 anchorPos, const std::string& cat) {
    open = true;
    justOpened = true;
    creating = false;
    category = cat;
    entries = NamedListRegistry::Get().GetList(category);
    newEntryBuf[0] = '\0';

    float height = 22.0f + (float)(entries.size() + 2) * 18.0f + 4.0f;
    area = {anchorPos.x, anchorPos.y, 200.0f, height};
}

void RegistryPickerPopup::Close() {
    open = false;
    creating = false;
}

bool RegistryPickerPopup::Render(std::string& outVal) {
    if (!open) return false;

    Vector2 mousePos = GetMousePosition();

    Rectangle areaCopy = area;
    deferredDraws.push_back([areaCopy]() {
        UIWidget::Panel(areaCopy);
    });

    float y = area.y + 2;

    // None entry, clears the reference
    Rectangle noneItem = {area.x + 1, y, area.width - 2, 18};
    bool noneHovered = !creating && CheckCollisionPointRec(mousePos, noneItem);
    deferredDraws.push_back([noneItem, noneHovered]() {
        const UIPalette& theme = UITheme::Get();
        if (noneHovered) DrawRectangleRec(noneItem, theme.surfaceHover);
        DrawText("None", (int)noneItem.x + 6, (int)noneItem.y + 3, 11, theme.textSecondary);
    });
    if (!creating && noneHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        outVal.clear();
        Close();
        return true;
    }
    y += 18.0f;

    // Registered entries
    for (size_t i = 0; i < entries.size(); i++) {
        Rectangle item = {area.x + 1, y, area.width - 2, 18};
        bool hovered = !creating && CheckCollisionPointRec(mousePos, item);
        std::string name = entries[i];
        deferredDraws.push_back([item, hovered, name]() {
            const UIPalette& theme = UITheme::Get();
            if (hovered) DrawRectangleRec(item, theme.surfaceHover);
            DrawText(name.c_str(), (int)item.x + 6, (int)item.y + 3, 11, theme.textPrimary);
        });

        if (!creating && hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            outVal = entries[i];
            Close();
            return true;
        }
        y += 18.0f;
    }

    // Inline create-new row
    Rectangle newItem = {area.x + 1, y, area.width - 2, 18};
    if (creating) {
        GuiTextBox(newItem, newEntryBuf, sizeof(newEntryBuf), true);
        if (IsKeyPressed(KEY_ENTER) && newEntryBuf[0] != '\0') {
            std::string name = newEntryBuf;
            NamedListRegistry::Get().AddEntry(category, name);
            outVal = name;
            Close();
            return true;
        }
        if (IsKeyPressed(KEY_ESCAPE)) creating = false;
    } else {
        bool newHovered = CheckCollisionPointRec(mousePos, newItem);
        deferredDraws.push_back([newItem, newHovered]() {
            const UIPalette& theme = UITheme::Get();
            if (newHovered) DrawRectangleRec(newItem, theme.surfaceHover);
            DrawText("+ New...", (int)newItem.x + 6, (int)newItem.y + 3, 11, theme.success);
        });
        if (newHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            creating = true;
            newEntryBuf[0] = '\0';
        }
    }

    if (justOpened) {
        justOpened = false;
    } else if (!creating && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, area)) {
        Close();
    }

    return false;
}

void RegistryPickerPopup::FlushDeferredDraws() {
    for (const auto& draw : deferredDraws) draw();
    deferredDraws.clear();
}