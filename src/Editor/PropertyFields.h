#pragma once
#include "raylib.h"
#include <string>

namespace PropertyField {

struct Style {
    Color box = {38, 38, 42, 255};
    Color border = {70, 70, 80, 255};
    Color text = WHITE;
    Color label = {150, 150, 160, 255};
};

bool FloatBox(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
              float& outVal, bool allowActivate = true, const Style& style = {});

bool IntBox(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
            int& outVal, bool allowActivate = true, const Style& style = {});

bool StringBox(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
               std::string& outVal, bool allowActivate = true, const char* placeholder = nullptr,
               const Style& style = {});

bool BoolBox(Rectangle r, bool& outVal);

// Three FloatBoxes side by side inside rowRect, labeled X/Y/Z, using fieldIdBase,
// fieldIdBase+1 and fieldIdBase+2.
bool Vec3Row(Rectangle rowRect, char bufX[32], char bufY[32], char bufZ[32], int fieldIdBase,
             int& activeField, float& outX, float& outY, float& outZ, bool allowActivate = true,
             const Style& style = {});

// Warms up raygui's shared cursor state as if this field had just been clicked, so
// the text cursor lands at the end of buf instead of wherever it was last left. Call
// this once, right when a field becomes active from something other than a click on
// the field itself (a "Rename" button, "+ Variable", and similar), before setting
// whatever flag your panel uses to track which field is now active.
void ForceActivate(char* buf, size_t bufSize);

}
