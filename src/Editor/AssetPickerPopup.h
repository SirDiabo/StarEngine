#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

class AssetPickerPopup {
public:
    void Open(Vector2 anchorPos, const std::string& extension);
    void OpenWithExtras(Vector2 anchorPos, const std::string& extension,
                         const std::vector<std::pair<std::string, std::string>>& extras);
    void Close();
    bool IsOpen() const { return open; }
    bool ContainsPoint(Vector2 point) const { return open && CheckCollisionPointRec(point, area); }
    bool Render(std::string& outPath);

    static void FlushDeferredDraws();

private:
    bool open = false;
    Rectangle area{};
    std::string extension;
    std::vector<std::string> displayNames;
    std::vector<std::string> contentPaths;

    bool hasPending = false;
    std::string pendingPath;
};