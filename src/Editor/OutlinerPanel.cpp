#include "OutlinerPanel.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"
#include "raylib.h"

void OutlinerPanel::Initialize() {
    name = "Outliner";
}

void OutlinerPanel::Render() {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(bounds);

    Vector2 mp = GetMousePosition();
    float y = bounds.y + 4;
    float rowH = 20.0f;

    if (!starMap) {
        DrawText("No map loaded", (int)bounds.x + 8, (int)y, 11, theme.textSecondary);
        return;
    }

    const auto& instances = starMap->GetInstances();
    for (size_t i = 0; i < instances.size(); i++) {
        const auto& inst = instances[i];
        Rectangle row = {bounds.x + 4, y, bounds.width - 8, rowH};

        bool isSelected = (selectedActorIndex && *selectedActorIndex == (int)i);
        bool hovered = CheckCollisionPointRec(mp, row);
        DrawRectangleRec(row, UIWidget::SelectableBg(hovered, isSelected, theme.panelBg));

        Color textColor = isSelected ? theme.textPrimary : theme.textSecondary;
        DrawText(inst.name.c_str(), (int)row.x + 8, (int)row.y + 4, 11, textColor);

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (selectedActorIndex) *selectedActorIndex = (int)i;
        }

        y += rowH;
    }
}