#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

class SearchableListPopup {
public:
    struct Entry {
        std::string category;
        std::string id;
        std::string label;
    };

    void Open(Vector2 anchorPos, float width = 200.0f, float maxVisibleHeight = 300.0f);
    void Close();
    bool IsOpen() const { return open; }
    bool ContainsPoint(Vector2 point) const { return open && CheckCollisionPointRec(point, lastRect); }

    std::string Render(const std::vector<Entry>& entries, Rectangle boundsArea);

    static void FlushDeferredDraws();

private:
    bool open = false;
    bool justOpened = false;
    Vector2 pos{0, 0};
    float width = 200.0f;
    float maxVisibleHeight = 300.0f;
    float scrollOffset = 0.0f;
    char search[64] = {};
    Rectangle lastRect{};

    static std::vector<std::function<void()>> deferredDraws;
};