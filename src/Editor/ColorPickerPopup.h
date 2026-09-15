#pragma once
#include "raylib.h"

class SliderElement;
class RectangleElement;
class TextElement;

class ColorPickerPopup {
public:
    void Open(Vector2 anchorPos, Color startColor);
    void Close();
    bool IsOpen() const { return open; }
    bool ContainsPoint(Vector2 point) const;

    bool Render(Color& outColor, bool& outConfirmed);

    Color GetLiveColor() const { return current; }

private:
    void SyncFromRGB();
    void SyncFromHSV();
    void SyncFromHex();
    void RefreshControls();

    bool open = false;
    Rectangle area{};

    Color original{};
    Color current{};

    float rf = 0.0f, gf = 0.0f, bf = 0.0f, af = 0.0f;
    float hh = 0.0f, ss = 0.0f, vv = 0.0f;
    char hexBuf[16] = "";

    bool hasPending = false;
    Color pendingColor{};
    bool pendingConfirmed = false;

    SliderElement* rSlider = nullptr;
    SliderElement* gSlider = nullptr;
    SliderElement* bSlider = nullptr;
    SliderElement* aSlider = nullptr;
    SliderElement* hSlider = nullptr;
    SliderElement* sSlider = nullptr;
    SliderElement* vSlider = nullptr;
    RectangleElement* newSwatchEl = nullptr;
    TextElement* hexBox = nullptr;
    TextElement* rValueText = nullptr;
    TextElement* gValueText = nullptr;
    TextElement* bValueText = nullptr;
    TextElement* aValueText = nullptr;
    TextElement* hValueText = nullptr;
    TextElement* sValueText = nullptr;
    TextElement* vValueText = nullptr;
};