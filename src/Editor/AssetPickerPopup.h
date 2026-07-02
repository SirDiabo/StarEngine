#pragma once
#include "raylib.h"
#include <string>
#include <vector>

// Small dropdown for picking a project content asset by file extension. Used wherever
// a component property references another asset (static mesh, material, texture) in
// panels that don't have a content browser of their own to drag-and-drop from.
class AssetPickerPopup {
public:
    void Open(Vector2 anchorPos, const std::string& extension);
    void Close();
    bool IsOpen() const { return open; }

    // Draws the popup if open. Returns true and fills outPath when the user picked an
    // entry ("None" picks an empty path). Returns false if nothing was picked this
    // frame, including when the popup is closed or the user clicked away from it.
    bool Render(std::string& outPath);

private:
    bool open = false;
    Rectangle area{};
    std::string extension;
    std::vector<std::string> displayNames;
    std::vector<std::string> fullPaths;
};
