#include "ConfirmPopup.h"

std::vector<std::function<void()>> ConfirmPopup::deferredDraws;

void ConfirmPopup::Open(const std::string& msg, const std::string& confirm) {
    open = true;
    justOpened = true;
    message = msg;
    confirmLabel = confirm;
}

void ConfirmPopup::Close() {
    open = false;
}

bool ConfirmPopup::Render(bool& outConfirmed) {
    if (!open) return false;

    Vector2 mousePos = GetMousePosition();
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float w = 340.0f;
    float h = 100.0f;
    area = {(sw - w) * 0.5f, (sh - h) * 0.5f, w, h};

    Rectangle cancelBtn  = {area.x + 16, area.y + area.height - 36, area.width * 0.5f - 24, 24};
    Rectangle confirmBtn = {area.x + area.width * 0.5f + 8, area.y + area.height - 36, area.width * 0.5f - 24, 24};

    bool cancelHovered = CheckCollisionPointRec(mousePos, cancelBtn);
    bool confirmHovered = CheckCollisionPointRec(mousePos, confirmBtn);

    Rectangle areaCopy = area;
    std::string msgCopy = message;
    std::string confirmLabelCopy = confirmLabel;
    deferredDraws.push_back([sw, sh, areaCopy, msgCopy, cancelBtn, confirmBtn, cancelHovered, confirmHovered, confirmLabelCopy]() {
        DrawRectangle(0, 0, (int)sw, (int)sh, {0, 0, 0, 130});
        DrawRectangleRounded(areaCopy, 0.08f, 6, {42, 42, 48, 255});
        DrawRectangleRoundedLinesEx(areaCopy, 0.08f, 6, 1.5f, {95, 95, 105, 255});

        int textW = MeasureText(msgCopy.c_str(), 13);
        DrawText(msgCopy.c_str(), (int)(areaCopy.x + (areaCopy.width - textW) * 0.5f), (int)areaCopy.y + 18, 13, WHITE);

        DrawRectangleRec(cancelBtn, cancelHovered ? Color{70, 70, 78, 255} : Color{55, 55, 62, 255});
        DrawRectangleLinesEx(cancelBtn, 1, {90, 90, 98, 255});
        int cw = MeasureText("Cancel", 12);
        DrawText("Cancel", (int)(cancelBtn.x + (cancelBtn.width - cw) * 0.5f), (int)cancelBtn.y + 6, 12, WHITE);

        DrawRectangleRec(confirmBtn, confirmHovered ? Color{190, 70, 70, 255} : Color{160, 55, 55, 255});
        DrawRectangleLinesEx(confirmBtn, 1, {210, 90, 90, 255});
        int fw = MeasureText(confirmLabelCopy.c_str(), 12);
        DrawText(confirmLabelCopy.c_str(), (int)(confirmBtn.x + (confirmBtn.width - fw) * 0.5f), (int)confirmBtn.y + 6, 12, WHITE);
    });

    if (justOpened) {
        justOpened = false;
        return false;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (confirmHovered) {
            Close();
            outConfirmed = true;
            return true;
        }
        if (cancelHovered) {
            Close();
            outConfirmed = false;
            return true;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        Close();
        outConfirmed = false;
        return true;
    }

    return false;
}

void ConfirmPopup::FlushDeferredDraws() {
    for (const auto& draw : deferredDraws) draw();
    deferredDraws.clear();
}
