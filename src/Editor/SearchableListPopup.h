#pragma once
#include "raylib.h"
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

    std::string Render(const std::vector<Entry>& entries, Rectangle boundsArea);

private:
    bool open = false;
    bool justOpened = false;
    Vector2 pos{0, 0};
    float width = 200.0f;
    float maxVisibleHeight = 300.0f;
    float scrollOffset = 0.0f;
    char search[64] = {};
};
