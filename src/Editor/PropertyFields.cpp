#include "PropertyFields.h"
#include "raygui.h"
#include <cstdio>
#include <cstdlib>
#include <functional>

namespace PropertyField {

static bool TextBoxCore(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
                         bool allowActivate, const char* placeholder, const Style& style,
                         const std::function<void()>& onActivate) {
    Vector2 mp = GetMousePosition();

    if (activeField == fieldId) {
        GuiTextBox(r, buf, (int)bufSize, true);
        if (IsKeyPressed(KEY_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, r))) {
            activeField = 0;
            return true;
        }
        return false;
    }

    bool clicked = allowActivate && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r);
    if (clicked) {
        if (onActivate) onActivate();
        GuiTextBox(r, buf, (int)bufSize, false);
    }
    DrawRectangleRec(r, style.box);
    DrawRectangleLinesEx(r, 1, style.border);
    if (buf[0] == '\0' && placeholder != nullptr) {
        DrawText(placeholder, (int)r.x + 4, (int)r.y + 3, 11, style.label);
    } else {
        DrawText(buf, (int)r.x + 4, (int)r.y + 3, 11, style.text);
    }
    if (clicked) activeField = fieldId;
    return false;
}

bool FloatBox(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
              float& outVal, bool allowActivate, const Style& style) {
    bool committed = TextBoxCore(r, buf, bufSize, fieldId, activeField, allowActivate, nullptr, style,
                                  [&]() { snprintf(buf, bufSize, "%.2f", outVal); });
    if (committed) {
        outVal = (float)atof(buf);
        return true;
    }
    return false;
}

bool IntBox(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
            int& outVal, bool allowActivate, const Style& style) {
    bool committed = TextBoxCore(r, buf, bufSize, fieldId, activeField, allowActivate, nullptr, style,
                                  [&]() { snprintf(buf, bufSize, "%d", outVal); });
    if (committed) {
        outVal = atoi(buf);
        return true;
    }
    return false;
}

bool StringBox(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
               std::string& outVal, bool allowActivate, const char* placeholder, const Style& style) {
    bool committed = TextBoxCore(r, buf, bufSize, fieldId, activeField, allowActivate, placeholder, style,
                                  [&]() { snprintf(buf, bufSize, "%s", outVal.c_str()); });
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

bool Vec3Row(Rectangle rowRect, char bufX[32], char bufY[32], char bufZ[32], int fieldIdBase,
             int& activeField, float& outX, float& outY, float& outZ, bool allowActivate,
             const Style& style) {
    float colW = rowRect.width / 3.0f;
    float boxW = colW - 14;
    bool changed = false;

    DrawText("X", (int)rowRect.x, (int)rowRect.y + 3, 11, style.label);
    changed |= FloatBox({rowRect.x + 14, rowRect.y, boxW, rowRect.height}, bufX, 32,
                         fieldIdBase + 0, activeField, outX, allowActivate, style);

    DrawText("Y", (int)(rowRect.x + colW), (int)rowRect.y + 3, 11, style.label);
    changed |= FloatBox({rowRect.x + colW + 14, rowRect.y, boxW, rowRect.height}, bufY, 32,
                         fieldIdBase + 1, activeField, outY, allowActivate, style);

    DrawText("Z", (int)(rowRect.x + colW * 2), (int)rowRect.y + 3, 11, style.label);
    changed |= FloatBox({rowRect.x + colW * 2 + 14, rowRect.y, boxW, rowRect.height}, bufZ, 32,
                         fieldIdBase + 2, activeField, outZ, allowActivate, style);

    return changed;
}

void ForceActivate(char* buf, size_t bufSize) {
    Vector2 mp = GetMousePosition();
    GuiTextBox({mp.x, mp.y, 1, 1}, buf, (int)bufSize, false);
}

}
