#pragma once
#include "raylib.h"

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
    void UpdateHexBuffer();

    bool open = false;
    bool justOpened = false;
    Rectangle area{};

    Color original{};
    Color current{};

    float rf = 0.0f, gf = 0.0f, bf = 0.0f, af = 0.0f;
    float hh = 0.0f, ss = 0.0f, vv = 0.0f;

    char hexBuf[16] = "";
    bool hexActive = false;

    int activeSlider = -1;
};