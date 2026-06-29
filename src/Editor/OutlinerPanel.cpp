#include "OutlinerPanel.h"
#include "raygui.h"
#include "raylib.h"

void OutlinerPanel::Initialize() {
    name = "Outliner";
}

void OutlinerPanel::Render() {
    DrawRectangleRec(bounds, {38, 38, 42, 255});
    DrawRectangleLinesEx(bounds, 1, {65, 65, 70, 255});

    DrawText("Outliner", (int)bounds.x + 8, (int)bounds.y + 8, 13, {160, 160, 170, 255});
    DrawLine((int)bounds.x + 4, (int)bounds.y + 24, (int)(bounds.x + bounds.width - 4), (int)bounds.y + 24, {60, 60, 65, 255});

    Vector2 mp = GetMousePosition();
    float y = bounds.y + 32;
    float rowH = 20.0f;

    if (!starMap) {
        DrawText("No map loaded", (int)bounds.x + 8, (int)y, 11, {100, 100, 110, 255});
        return;
    }

    const auto& instances = starMap->GetInstances();
    for (size_t i = 0; i < instances.size(); i++) {
        const auto& inst = instances[i];
        Rectangle row = {bounds.x + 4, y, bounds.width - 8, rowH};

        bool isSelected = (selectedActorIndex && *selectedActorIndex == (int)i);
        if (isSelected) {
            DrawRectangleRec(row, {55, 60, 80, 255});
        }

        Color textColor = isSelected ? WHITE : Color{140, 140, 150, 255};
        DrawText(inst.name.c_str(), (int)row.x + 8, (int)row.y + 4, 11, textColor);

        if (CheckCollisionPointRec(mp, row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (selectedActorIndex) *selectedActorIndex = (int)i;
        }

        y += rowH;
    }
}