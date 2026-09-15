#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

class SearchListElement;

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
    Vector2 pos{0, 0};
    float width = 200.0f;
    float maxVisibleHeight = 300.0f;
    Rectangle lastRect{};

    SearchListElement* content = nullptr;
    bool hasPending = false;
    std::string pendingId;
};