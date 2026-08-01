#include "PropertyFields.h"
#include "UIWidgets.h"
#include "raygui.h"
#include "Modules/Stargraph/Stargraph.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <cmath>

namespace PropertyField {

static int g_activeField = 0;

static DragState g_drag;

static float ApplyDrag(float currentVal, float startVal) {
    if (!g_drag.active) return currentVal;
    int dx = GetMouseX() - g_drag.startMouseX;
    float delta = dx * 0.1f;
    float scale = 1.0f;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) scale = 0.1f;
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) scale = 10.0f;
    return startVal + delta * scale;
}

static bool TextBoxCore(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
                         bool allowActivate, const char* placeholder, const char* inactiveDisplay,
                         const Style& style, const std::function<void()>& onActivate) {
    Vector2 mp = GetMousePosition();

    bool isActive = (activeField == fieldId) && (g_activeField == 0 || g_activeField == fieldId) && allowActivate;
    if (activeField == fieldId && !isActive) {
        activeField = 0;
        if (g_activeField == fieldId) g_activeField = 0;
    }

    if (isActive) {
        GuiTextBox(r, buf, (int)bufSize, true);
        if (IsKeyPressed(KEY_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, r))) {
            activeField = 0;
            if (g_activeField == fieldId) g_activeField = 0;
            return true;
        }
        return false;
    }

    const char* display = inactiveDisplay ? inactiveDisplay : buf;

    bool clicked = allowActivate && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r);
    if (clicked) {
        if (onActivate) onActivate();
        activeField = fieldId;
        g_activeField = fieldId;
    }
    DrawRectangleRec(r, style.box);
    DrawRectangleLinesEx(r, 1, style.border);
    if (display[0] == '\0' && placeholder != nullptr) {
        DrawText(placeholder, (int)r.x + 4, (int)r.y + 3, 11, style.label);
    } else {
        DrawText(display, (int)r.x + 4, (int)r.y + 3, 11, style.text);
    }
    return false;
}

bool FloatBox(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
              float& outVal, bool allowActivate, const Style& style) {
    bool committed = TextBoxCore(r, buf, bufSize, fieldId, activeField, allowActivate, nullptr, nullptr, style,
                                  [&]() { snprintf(buf, bufSize, "%.2f", outVal); });

    if (g_drag.active && g_drag.fieldId == fieldId) {
        if (activeField != fieldId) {
            g_drag.active = false;
            g_drag.fieldId = -1;
        } else {
            outVal = ApplyDrag(outVal, g_drag.startVal);
            snprintf(buf, bufSize, "%.2f", outVal);
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                g_drag.active = false;
                g_drag.fieldId = -1;
            }
        }
    }

    if (committed) {
        outVal = (float)atof(buf);
        return true;
    }

    if (!g_drag.active && activeField == fieldId && allowActivate &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), r)) {
        g_drag.active = true;
        g_drag.fieldId = fieldId;
        g_drag.startVal = outVal;
        g_drag.startMouseX = GetMouseX();
    }

    return false;
}

bool IntBox(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
            int& outVal, bool allowActivate, const Style& style) {
    bool committed = TextBoxCore(r, buf, bufSize, fieldId, activeField, allowActivate, nullptr, nullptr, style,
                                  [&]() { snprintf(buf, bufSize, "%d", outVal); });

    if (g_drag.active && g_drag.fieldId == fieldId) {
        if (activeField != fieldId) {
            g_drag.active = false;
            g_drag.fieldId = -1;
        } else {
            outVal = (int)roundf(ApplyDrag((float)outVal, g_drag.startVal));
            snprintf(buf, bufSize, "%d", outVal);
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                g_drag.active = false;
                g_drag.fieldId = -1;
            }
        }
    }

    if (committed) {
        outVal = atoi(buf);
        return true;
    }

    if (!g_drag.active && activeField == fieldId && allowActivate &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), r)) {
        g_drag.active = true;
        g_drag.fieldId = fieldId;
        g_drag.startVal = (float)outVal;
        g_drag.startMouseX = GetMouseX();
    }

    return false;
}

bool StringBox(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
               std::string& outVal, bool allowActivate, const char* placeholder,
               const char* inactiveDisplay, const Style& style) {
    bool committed = TextBoxCore(r, buf, bufSize, fieldId, activeField, allowActivate, placeholder, inactiveDisplay,
                                  style, [&]() { snprintf(buf, bufSize, "%s", outVal.c_str()); });
    if (committed) {
        outVal = buf;
        return true;
    }
    return false;
}

bool StringBox(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
               PinValue& outVal, bool allowActivate, const char* placeholder,
               const char* inactiveDisplay, const Style& style) {
    bool committed = TextBoxCore(r, buf, bufSize, fieldId, activeField, allowActivate, placeholder, inactiveDisplay,
                                  style, [&]() { snprintf(buf, bufSize, "%s", outVal.c_str()); });
    if (committed) {
        outVal = buf;
        return true;
    }
    return false;
}

bool BoolBox(Rectangle r, bool& outVal) {
    bool before = outVal;
    GuiCheckBox(r, "", &outVal);
    return outVal != before;
}

bool ColorField(Rectangle r, const char* label, Color value, const Style& style) {
    float swatchSize = r.height - 4;
    Rectangle swatch = {r.x + r.width - swatchSize - 2, r.y + 2, swatchSize, swatchSize};

    DrawText(label, (int)r.x + 2, (int)(r.y + (r.height - 11) / 2.0f), 11, style.label);

    DrawRectangleRec(swatch, value);
    bool hovered = CheckCollisionPointRec(GetMousePosition(), swatch);
    DrawRectangleLinesEx(swatch, hovered ? 2 : 1, hovered ? style.text : style.border);

    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static const Color AxisColorX = {220, 65, 65, 255};
static const Color AxisColorY = {95, 200, 95, 255};
static const Color AxisColorZ = {70, 140, 230, 255};

bool Vec3Row(Rectangle rowRect, char bufX[32], char bufY[32], char bufZ[32], int fieldIdBase,
             int& activeField, float& outX, float& outY, float& outZ, bool allowActivate,
             const Style& style) {
    float colW = rowRect.width / 3.0f;
    float boxW = colW - 14;
    bool changed = false;

    DrawText("X", (int)rowRect.x, (int)rowRect.y + 3, 11, AxisColorX);
    changed |= FloatBox({rowRect.x + 14, rowRect.y, boxW, rowRect.height}, bufX, 32,
                         fieldIdBase + 0, activeField, outX, allowActivate, style);

    DrawText("Y", (int)(rowRect.x + colW), (int)rowRect.y + 3, 11, AxisColorY);
    changed |= FloatBox({rowRect.x + colW + 14, rowRect.y, boxW, rowRect.height}, bufY, 32,
                         fieldIdBase + 1, activeField, outY, allowActivate, style);

    DrawText("Z", (int)(rowRect.x + colW * 2), (int)rowRect.y + 3, 11, AxisColorZ);
    changed |= FloatBox({rowRect.x + colW * 2 + 14, rowRect.y, boxW, rowRect.height}, bufZ, 32,
                         fieldIdBase + 2, activeField, outZ, allowActivate, style);

    return changed;
}

void ForceActivate(char* buf, size_t bufSize) {
    Vector2 mp = GetMousePosition();
    GuiTextBox({mp.x, mp.y, 1, 1}, buf, (int)bufSize, false);
}

static const float kArrayRowHeight = 18.0f;
static const float kArrayBtnW = 16.0f;
static const float kArrayBtnGap = 2.0f;
static const float kArrayControlsWidth = kArrayBtnW * 3 + kArrayBtnGap * 2;

float ArrayControlsWidth() { return kArrayControlsWidth; }
float ArrayRowHeight() { return kArrayRowHeight; }

void ArrayHeader(Rectangle r, const char* label, int count, ArrayAction& action, const Style& style) {
    char text[80];
    snprintf(text, sizeof(text), "%s (%d)", label, count);
    DrawText(text, (int)r.x, (int)r.y + 3, 11, style.label);

    Rectangle addBtn = {r.x + r.width - kArrayBtnW, r.y, kArrayBtnW, r.height};
    if (UIWidget::Button(addBtn, "+")) action.addClicked = true;
}

void ArrayRowControls(Rectangle rowRect, int index, int count, ArrayAction& action, const Style& style) {
    (void)style;
    float x = rowRect.x + rowRect.width - kArrayControlsWidth;
    Rectangle upBtn = {x, rowRect.y, kArrayBtnW, rowRect.height};
    Rectangle downBtn = {x + kArrayBtnW + kArrayBtnGap, rowRect.y, kArrayBtnW, rowRect.height};
    Rectangle removeBtn = {x + (kArrayBtnW + kArrayBtnGap) * 2, rowRect.y, kArrayBtnW, rowRect.height};

    bool canUp = index > 0;
    bool canDown = index < count - 1;

    if (UIWidget::Button(upBtn, "^", canUp)) action.moveUpIndex = index;
    if (UIWidget::Button(downBtn, "v", canDown)) action.moveDownIndex = index;
    if (UIWidget::Button(removeBtn, "x")) action.removeIndex = index;
}

Rectangle ArrayValueRect(Rectangle rowRect) {
    return {rowRect.x, rowRect.y, rowRect.width - kArrayControlsWidth - 4, rowRect.height};
}

}