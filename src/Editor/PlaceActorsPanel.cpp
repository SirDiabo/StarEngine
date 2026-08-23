#include "PlaceActorsPanel.h"
#include "UITheme.h"
#include "UIWidgets.h"

void PlaceActorsPanel::Initialize() {
    name = "Place Actors";
}

void PlaceActorsPanel::Render() {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(bounds);

    Vector2 mousePos = GetMousePosition();
    const std::vector<BasicActorDef>& defs = GetBasicActorDefs();

    DrawText("Place Actors", (int)bounds.x + 8, (int)bounds.y + 6, 12, theme.textSecondary);

    float rowHeight = 30.0f;
    float y = bounds.y + 26;

    for (size_t i = 0; i < defs.size(); i++) {
        Rectangle row = {bounds.x + 4, y, bounds.width - 8, rowHeight - 4};
        bool hovered = CheckCollisionPointRec(mousePos, row);

        DrawRectangleRounded(row, 0.15f, 4, UIWidget::SelectableBg(hovered, false, theme.buttonBg));

        Rectangle iconRect = {row.x + 6, row.y + 5, rowHeight - 14, rowHeight - 14};
        DrawRectangleRounded(iconRect, 0.25f, 4, defs[i].accentColor);

        DrawText(defs[i].label.c_str(), (int)iconRect.x + (int)iconRect.width + 8,
                  (int)row.y + (int)(row.height * 0.5f) - 5, 11, theme.textPrimary);

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            dragIndex = (int)i;
            dragPressPos = mousePos;
        }

        if (dragIndex == (int)i && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float dx = mousePos.x - dragPressPos.x;
            float dy = mousePos.y - dragPressPos.y;
            if (dx * dx + dy * dy > 36) dragging = true;
        }

        y += rowHeight;
    }

    if (dragging) {
        if (dragIndex >= 0 && dragIndex < (int)defs.size()) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                const BasicActorDef& def = defs[dragIndex];
                int lw = MeasureText(def.label.c_str(), 12) + 16;
                DrawRectangleRec({mousePos.x + 10, mousePos.y - 2, (float)lw, 18}, {55, 55, 75, 220});
                DrawRectangleLinesEx({mousePos.x + 10, mousePos.y - 2, (float)lw, 18}, 1, {120, 120, 200, 255});
                DrawRectangleRec({mousePos.x + 14, mousePos.y + 2, 10, 10}, def.accentColor);
                DrawText(def.label.c_str(), (int)mousePos.x + 28, (int)mousePos.y + 1, 12, WHITE);
            } else {
                if (onActorDragged) onActorDragged(defs[dragIndex], mousePos);
                dragging = false;
                dragIndex = -1;
            }
        } else {
            dragging = false;
            dragIndex = -1;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !dragging) dragIndex = -1;
}
