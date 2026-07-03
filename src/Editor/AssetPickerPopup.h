#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

class AssetPickerPopup {
public:
    void Open(Vector2 anchorPos, const std::string& extension);
    void Close();
    bool IsOpen() const { return open; }
    bool Render(std::string& outPath);

    static void FlushDeferredDraws();

private:
    bool open = false;
    bool justOpened = false;
    Rectangle area{};
    std::string extension;
    std::vector<std::string> displayNames;
    std::vector<std::string> fullPaths;

    static std::vector<std::function<void()>> deferredDraws;
};