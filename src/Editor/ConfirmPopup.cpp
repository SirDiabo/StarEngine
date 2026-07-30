#include "ConfirmPopup.h"
#include "UITheme.h"

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
        const UIPalette& theme = UITheme::Get();

        DrawRectangle(0, 0, (int)sw, (int)sh, {0, 0, 0, 130});
        DrawRectangleRounded(areaCopy, 0.08f, 6, theme.panelBg);
        DrawRectangleRoundedLinesEx(areaCopy, 0.08f, 6, 1.5f, theme.panelBorder);

        int textW = MeasureText(msgCopy.c_str(), 13);
        DrawText(msgCopy.c_str(), (int)(areaCopy.x + (areaCopy.width - textW) * 0.5f), (int)areaCopy.y + 18, 13, theme.textPrimary);

        DrawRectangleRec(cancelBtn, cancelHovered ? theme.buttonHover : theme.buttonBg);
        DrawRectangleLinesEx(cancelBtn, 1, theme.buttonBorder);
        int cw = MeasureText("Cancel", 12);
        DrawText("Cancel", (int)(cancelBtn.x + (cancelBtn.width - cw) * 0.5f), (int)cancelBtn.y + 6, 12, theme.buttonText);

        DrawRectangleRec(confirmBtn, confirmHovered ? theme.danger : theme.dangerBg);
        DrawRectangleLinesEx(confirmBtn, 1, theme.danger);
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