#pragma once
#include "raylib.h"
#include <string>

class PinValue;

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
               const char* inactiveDisplay = nullptr, const Style& style = {});

bool StringBox(Rectangle r, char* buf, size_t bufSize, int fieldId, int& activeField,
               PinValue& outVal, bool allowActivate = true, const char* placeholder = nullptr,
               const char* inactiveDisplay = nullptr, const Style& style = {});

bool BoolBox(Rectangle r, bool& outVal);

bool Vec3Row(Rectangle rowRect, char bufX[32], char bufY[32], char bufZ[32], int fieldIdBase,
             int& activeField, float& outX, float& outY, float& outZ, bool allowActivate = true,
             const Style& style = {});

void ForceActivate(char* buf, size_t bufSize);

struct ArrayAction {
    bool addClicked = false;
    int removeIndex = -1;
    int moveUpIndex = -1;
    int moveDownIndex = -1;

    bool Any() const { return addClicked || removeIndex >= 0 || moveUpIndex >= 0 || moveDownIndex >= 0; }
};

float ArrayControlsWidth();
float ArrayRowHeight();
void ArrayHeader(Rectangle r, const char* label, int count, ArrayAction& action, const Style& style = {});
void ArrayRowControls(Rectangle rowRect, int index, int count, ArrayAction& action, const Style& style = {});

Rectangle ArrayValueRect(Rectangle rowRect);

struct DragState {
    bool active = false;
    int fieldId = -1;
    float startVal = 0.0f;
    int startMouseX = 0;
    };
}