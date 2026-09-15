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
    Rectangle area{};
    std::vector<ChoiceOption> options;

    bool hasPending = false;
    int pendingIndex = -1;
};
