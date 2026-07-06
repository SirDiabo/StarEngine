#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

struct ChoiceOption {
    std::string label;
    bool enabled = true;
    std::string disabledReason;
};

class ChoicePopup {
public:
    void Open(Vector2 anchorPos, float width, const std::vector<ChoiceOption>& choices);
    void Close();
    bool IsOpen() const { return open; }
    bool ContainsPoint(Vector2 point) const { return open && CheckCollisionPointRec(point, area); }
    bool Render(int& outIndex);

    static void FlushDeferredDraws();

private:
    bool open = false;
    bool justOpened = false;
    Rectangle area{};
    std::vector<ChoiceOption> options;

    static std::vector<std::function<void()>> deferredDraws;
};
