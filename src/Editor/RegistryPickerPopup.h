#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

class RegistryPickerPopup {
public:
    void Open(Vector2 anchorPos, const std::string& category);
    void OpenList(Vector2 anchorPos, const std::vector<std::string>& names, const std::string& noneLabel);
    void Close();
    bool IsOpen() const { return open; }
    bool ContainsPoint(Vector2 point) const { return open && CheckCollisionPointRec(point, area); }
    bool Render(std::string& outVal);

    static void FlushDeferredDraws();

private:
    void OpenInternal(Vector2 anchorPos, const std::string& noneLabel, bool allowCreate);

    bool open = false;
    Rectangle area{};
    std::string category;
    std::vector<std::string> entries;

    bool hasPending = false;
    std::string pendingValue;
};
