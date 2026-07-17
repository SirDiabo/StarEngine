#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

class RegistryPickerPopup {
public:
    void Open(Vector2 anchorPos, const std::string& category);
    void Close();
    bool IsOpen() const { return open; }
    bool ContainsPoint(Vector2 point) const { return open && CheckCollisionPointRec(point, area); }
    bool Render(std::string& outVal);

    static void FlushDeferredDraws();

private:
    bool open = false;
    bool justOpened = false;
    bool creating = false;
    Rectangle area{};
    std::string category;
    std::vector<std::string> entries;
    char newEntryBuf[64] = {};

    static std::vector<std::function<void()>> deferredDraws;
};
