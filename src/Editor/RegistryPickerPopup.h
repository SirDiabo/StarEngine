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
    Rectangle area{};
    std::string category;
    std::vector<std::string> entries;

    bool hasPending = false;
    std::string pendingValue;
};
