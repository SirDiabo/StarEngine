#pragma once
#include "raylib.h"
#include <string>

class TrustProjectPopup {
public:
    void Open(const std::string& projectName, const std::string& projectRoot);
    void Close();
    bool IsOpen() const { return open; }
    bool ContainsPoint(Vector2 point) const { (void)point; return open; }
    bool Render(bool& outTrusted);

private:
    bool open = false;
    Rectangle area{};

    bool hasPending = false;
    bool pendingTrusted = false;
};
