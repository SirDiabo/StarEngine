#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

class ConfirmPopup {
public:
    void Open(const std::string& message, const std::string& confirmLabel = "Delete");
    void Close();
    bool IsOpen() const { return open; }
    bool ContainsPoint(Vector2 point) const { (void)point; return open; }
    bool Render(bool& outConfirmed);

    static void FlushDeferredDraws();

private:
    bool open = false;
    bool justOpened = false;
    std::string message;
    std::string confirmLabel;
    Rectangle area{};

    static std::vector<std::function<void()>> deferredDraws;
};
