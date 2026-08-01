#include "ColorPickerPopup.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const float POPUP_W = 260.0f;
static const float POPUP_H = 320.0f;

static bool DragSlider(Rectangle r, float& value01, int sliderId, int& activeSlider) {
    Vector2 mp = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mp, r);
    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) activeSlider = sliderId;

    bool changed = false;
    if (activeSlider == sliderId) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float nv = (mp.x - r.x) / r.width;
            if (nv < 0.0f) nv = 0.0f;
            if (nv > 1.0f) nv = 1.0f;
            if (nv != value01) { value01 = nv; changed = true; }
        } else {
            activeSlider = -1;
        }
    }

    float hx = r.x + value01 * r.width;
    DrawRectangleLinesEx({hx - 2, r.y - 2, 4, r.height + 4}, 2, WHITE);
    DrawRectangleLinesEx({hx - 2, r.y - 2, 4, r.height + 4}, 1, BLACK);
    return changed;
}

static void DrawHueGradient(Rectangle r) {
    static const Color stops[7] = {
        {255, 0, 0, 255}, {255, 255, 0, 255}, {0, 255, 0, 255}, {0, 255, 255, 255},
        {0, 0, 255, 255}, {255, 0, 255, 255}, {255, 0, 0, 255},
    };
    float segW = r.width / 6.0f;
    for (int i = 0; i < 6; i++) {
        DrawRectangleGradientH((int)(r.x + i * segW), (int)r.y, (int)segW + 1, (int)r.height, stops[i], stops[i + 1]);
    }
}

void ColorPickerPopup::Open(Vector2 anchorPos, Color startColor) {
    open = true;
    justOpened = true;
    activeSlider = -1;
    hexActive = false;

    original = startColor;
    current = startColor;
    rf = startColor.r / 255.0f;
    gf = startColor.g / 255.0f;
    bf = startColor.b / 255.0f;
    af = startColor.a / 255.0f;

    Vector3 hsv = ColorToHSV(startColor);
    hh = hsv.x;
    ss = hsv.y;
    vv = hsv.z;
    UpdateHexBuffer();

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float x = anchorPos.x;
    float y = anchorPos.y;
    if (x + POPUP_W > screenW) x = screenW - POPUP_W - 4;
    if (y + POPUP_H > screenH) y = screenH - POPUP_H - 4;
    area = {x, y, POPUP_W, POPUP_H};
}

void ColorPickerPopup::Close() {
    open = false;
}

bool ColorPickerPopup::ContainsPoint(Vector2 point) const {
    return open && CheckCollisionPointRec(point, area);
}

void ColorPickerPopup::UpdateHexBuffer() {
    snprintf(hexBuf, sizeof(hexBuf), "%02X%02X%02X%02X", current.r, current.g, current.b, current.a);
}

void ColorPickerPopup::SyncFromRGB() {
    current = {(unsigned char)(rf * 255.0f), (unsigned char)(gf * 255.0f),
               (unsigned char)(bf * 255.0f), (unsigned char)(af * 255.0f)};
    Vector3 hsv = ColorToHSV(current);
    hh = hsv.x;
    ss = hsv.y;
    vv = hsv.z;
    UpdateHexBuffer();
}

void ColorPickerPopup::SyncFromHSV() {
    Color rgb = ColorFromHSV(hh, ss, vv);
    current = {rgb.r, rgb.g, rgb.b, (unsigned char)(af * 255.0f)};
    rf = current.r / 255.0f;
    gf = current.g / 255.0f;
    bf = current.b / 255.0f;
    UpdateHexBuffer();
}

void ColorPickerPopup::SyncFromHex() {
    unsigned long value = strtoul(hexBuf, nullptr, 16);
    size_t len = strlen(hexBuf);

    unsigned char r, g, b, a;
    if (len <= 6) {
        r = (unsigned char)((value >> 16) & 0xFF);
        g = (unsigned char)((value >> 8) & 0xFF);
        b = (unsigned char)(value & 0xFF);
        a = 255;
    } else {
        r = (unsigned char)((value >> 24) & 0xFF);
        g = (unsigned char)((value >> 16) & 0xFF);
        b = (unsigned char)((value >> 8) & 0xFF);
        a = (unsigned char)(value & 0xFF);
    }

    current = {r, g, b, a};
    rf = r / 255.0f;
    gf = g / 255.0f;
    bf = b / 255.0f;
    af = a / 255.0f;
    Vector3 hsv = ColorToHSV(current);
    hh = hsv.x;
    ss = hsv.y;
    vv = hsv.z;
}

bool ColorPickerPopup::Render(Color& outColor, bool& outConfirmed) {
    if (!open) return false;

    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(area);

    float fx = area.x + 10;
    float fy = area.y + 10;
    float contentW = area.width - 20;
    Vector2 mp = GetMousePosition();

    float swatchW = (contentW - 8) / 2.0f;
    Rectangle oldSwatch = {fx, fy + 12, swatchW, 34};
    Rectangle newSwatch = {fx + swatchW + 8, fy + 12, swatchW, 34};
    DrawText("Old", (int)oldSwatch.x, (int)fy, 10, theme.textSecondary);
    DrawText("New", (int)newSwatch.x, (int)fy, 10, theme.textSecondary);
    DrawRectangleRec(oldSwatch, original);
    DrawRectangleLinesEx(oldSwatch, 1, theme.fieldBorder);
    DrawRectangleRec(newSwatch, current);
    DrawRectangleLinesEx(newSwatch, 1, theme.fieldBorder);
    fy += 12 + 34 + 10;

    float labelW = 14.0f;
    float valueW = 40.0f;
    Rectangle track = {fx + labelW, fy, contentW - labelW - valueW, 18};
    char valBuf[16];

    DrawText("R", (int)fx, (int)fy + 3, 11, theme.textSecondary);
    DrawRectangleGradientH((int)track.x, (int)track.y, (int)track.width, (int)track.height,
                            {0, (unsigned char)(gf * 255), (unsigned char)(bf * 255), 255},
                            {255, (unsigned char)(gf * 255), (unsigned char)(bf * 255), 255});
    DrawRectangleLinesEx(track, 1, theme.fieldBorder);
    if (DragSlider(track, rf, 0, activeSlider)) SyncFromRGB();
    snprintf(valBuf, sizeof(valBuf), "%.2f", rf);
    DrawText(valBuf, (int)(fx + contentW - valueW + 4), (int)fy + 3, 11, theme.textPrimary);
    fy += 22;

    track.y = fy;
    DrawText("G", (int)fx, (int)fy + 3, 11, theme.textSecondary);
    DrawRectangleGradientH((int)track.x, (int)track.y, (int)track.width, (int)track.height,
                            {(unsigned char)(rf * 255), 0, (unsigned char)(bf * 255), 255},
                            {(unsigned char)(rf * 255), 255, (unsigned char)(bf * 255), 255});
    DrawRectangleLinesEx(track, 1, theme.fieldBorder);
    if (DragSlider(track, gf, 1, activeSlider)) SyncFromRGB();
    snprintf(valBuf, sizeof(valBuf), "%.2f", gf);
    DrawText(valBuf, (int)(fx + contentW - valueW + 4), (int)fy + 3, 11, theme.textPrimary);
    fy += 22;

    track.y = fy;
    DrawText("B", (int)fx, (int)fy + 3, 11, theme.textSecondary);
    DrawRectangleGradientH((int)track.x, (int)track.y, (int)track.width, (int)track.height,
                            {(unsigned char)(rf * 255), (unsigned char)(gf * 255), 0, 255},
                            {(unsigned char)(rf * 255), (unsigned char)(gf * 255), 255, 255});
    DrawRectangleLinesEx(track, 1, theme.fieldBorder);
    if (DragSlider(track, bf, 2, activeSlider)) SyncFromRGB();
    snprintf(valBuf, sizeof(valBuf), "%.2f", bf);
    DrawText(valBuf, (int)(fx + contentW - valueW + 4), (int)fy + 3, 11, theme.textPrimary);
    fy += 22;

    track.y = fy;
    DrawText("A", (int)fx, (int)fy + 3, 11, theme.textSecondary);
    DrawRectangleGradientH((int)track.x, (int)track.y, (int)track.width, (int)track.height,
                            {(unsigned char)(rf * 255), (unsigned char)(gf * 255), (unsigned char)(bf * 255), 0},
                            {(unsigned char)(rf * 255), (unsigned char)(gf * 255), (unsigned char)(bf * 255), 255});
    DrawRectangleLinesEx(track, 1, theme.fieldBorder);
    if (DragSlider(track, af, 3, activeSlider)) SyncFromRGB();
    snprintf(valBuf, sizeof(valBuf), "%.2f", af);
    DrawText(valBuf, (int)(fx + contentW - valueW + 4), (int)fy + 3, 11, theme.textPrimary);
    fy += 26;

    float hueNorm = hh / 360.0f;
    track.y = fy;
    DrawText("H", (int)fx, (int)fy + 3, 11, theme.textSecondary);
    DrawHueGradient(track);
    DrawRectangleLinesEx(track, 1, theme.fieldBorder);
    if (DragSlider(track, hueNorm, 4, activeSlider)) {
        hh = hueNorm * 360.0f;
        SyncFromHSV();
    }
    snprintf(valBuf, sizeof(valBuf), "%.0f", hh);
    DrawText(valBuf, (int)(fx + contentW - valueW + 4), (int)fy + 3, 11, theme.textPrimary);
    fy += 22;

    track.y = fy;
    DrawText("S", (int)fx, (int)fy + 3, 11, theme.textSecondary);
    DrawRectangleGradientH((int)track.x, (int)track.y, (int)track.width, (int)track.height,
                            ColorFromHSV(hh, 0.0f, vv), ColorFromHSV(hh, 1.0f, vv));
    DrawRectangleLinesEx(track, 1, theme.fieldBorder);
    if (DragSlider(track, ss, 5, activeSlider)) SyncFromHSV();
    snprintf(valBuf, sizeof(valBuf), "%.2f", ss);
    DrawText(valBuf, (int)(fx + contentW - valueW + 4), (int)fy + 3, 11, theme.textPrimary);
    fy += 22;

    track.y = fy;
    DrawText("V", (int)fx, (int)fy + 3, 11, theme.textSecondary);
    DrawRectangleGradientH((int)track.x, (int)track.y, (int)track.width, (int)track.height,
                            ColorFromHSV(hh, ss, 0.0f), ColorFromHSV(hh, ss, 1.0f));
    DrawRectangleLinesEx(track, 1, theme.fieldBorder);
    if (DragSlider(track, vv, 6, activeSlider)) SyncFromHSV();
    snprintf(valBuf, sizeof(valBuf), "%.2f", vv);
    DrawText(valBuf, (int)(fx + contentW - valueW + 4), (int)fy + 3, 11, theme.textPrimary);
    fy += 28;

    DrawText("Hex", (int)fx, (int)fy + 4, 11, theme.textSecondary);
    Rectangle hexBox = {fx + 32, fy, contentW - 32, 20};
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, hexBox)) hexActive = true;
    GuiTextBox(hexBox, hexBuf, sizeof(hexBuf), hexActive);
    if (hexActive && IsKeyPressed(KEY_ENTER)) {
        SyncFromHex();
        hexActive = false;
    }
    if (hexActive && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, hexBox)) {
        SyncFromHex();
        hexActive = false;
    }
    fy += 30;

    float btnW = (contentW - 8) / 2.0f;
    Rectangle cancelBtn = {fx, area.y + area.height - 34, btnW, 24};
    Rectangle confirmBtn = {fx + btnW + 8, area.y + area.height - 34, btnW, 24};

    bool decided = false;
    if (UIWidget::Button(cancelBtn, "Cancel")) {
        outConfirmed = false;
        decided = true;
        Close();
    }
    if (UIWidget::Button(confirmBtn, "Confirm")) {
        outColor = current;
        outConfirmed = true;
        decided = true;
        Close();
    }
    if (decided) return true;

    if (justOpened) {
        justOpened = false;
    } else if (!hexActive && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, area)) {
        Close();
    }

    return false;
}
