#include "DetailsPanel.h"
#include "raygui.h"
#include "raylib.h"
#include <cstdio>

void DetailsPanel::Initialize() {
    name = "Details";
}

void DetailsPanel::Render() {
    DrawRectangleRec(bounds, {38, 38, 42, 255});
    DrawRectangleLinesEx(bounds, 1, {65, 65, 70, 255});

    float x = bounds.x;
    float y = bounds.y + 8;
    float w = bounds.width;
    Vector2 mp = GetMousePosition();

    if (!starMap || !selectedActorIndex || *selectedActorIndex < 0 ||
        *selectedActorIndex >= (int)starMap->GetInstances().size()) {
        DrawText("Nothing selected", (int)x + 8, (int)y, 11, {100, 100, 110, 255});
        return;
    }

    ActorInstance& inst = const_cast<ActorInstance&>(starMap->GetInstances()[*selectedActorIndex]);

    if (lastActorIndex != *selectedActorIndex) {
        snprintf(nameEditBuf, sizeof(nameEditBuf), "%s", inst.name.c_str());
        snprintf(posXBuf, sizeof(posXBuf), "%.2f", inst.posX);
        snprintf(posYBuf, sizeof(posYBuf), "%.2f", inst.posY);
        snprintf(posZBuf, sizeof(posZBuf), "%.2f", inst.posZ);
        lastActorIndex = *selectedActorIndex;
        activeField = 0;
    }

    auto labeledField = [&](const char* label, char* buf, size_t bufSize, int fieldId, float& outVal) {
        DrawText(label, (int)x + 8, (int)y, 11, {140, 140, 150, 255});
        y += 14;
        Rectangle r = {x + 8, y, w - 16, 18};
        if (activeField == fieldId) {
            GuiTextBox(r, buf, (int)bufSize, true);
            if (IsKeyPressed(KEY_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, r))) {
                outVal = (float)atof(buf);
                activeField = 0;
            }
        } else {
            DrawRectangleRec(r, {30, 30, 32, 255});
            DrawRectangleLinesEx(r, 1, {65, 65, 75, 255});
            DrawText(buf, (int)r.x + 4, (int)r.y + 3, 11, WHITE);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r)) {
                activeField = fieldId;
                snprintf(buf, bufSize, "%.2f", outVal);
            }
        }
        y += 22;
    };

    DrawText(inst.className.c_str(), (int)x + 8, (int)y, 13, WHITE);
    y += 20;
    DrawText("Name", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    Rectangle nameRect = {x + 8, y, w - 16, 18};
    if (activeField == 1) {
        GuiTextBox(nameRect, nameEditBuf, sizeof(nameEditBuf), true);
        if (IsKeyPressed(KEY_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, nameRect))) {
            inst.name = nameEditBuf;
            activeField = 0;
        }
    } else {
        DrawRectangleRec(nameRect, {30, 30, 32, 255});
        DrawRectangleLinesEx(nameRect, 1, {65, 65, 75, 255});
        DrawText(nameEditBuf, (int)nameRect.x + 4, (int)nameRect.y + 3, 11, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, nameRect))
            activeField = 1;
    }
    y += 26;

    DrawText("Position", (int)x + 8, (int)y, 11, {140, 140, 150, 255});
    y += 14;
    labeledField("X", posXBuf, sizeof(posXBuf), 2, inst.posX);
    labeledField("Y", posYBuf, sizeof(posYBuf), 3, inst.posY);
    labeledField("Z", posZBuf, sizeof(posZBuf), 4, inst.posZ);
}