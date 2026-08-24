#pragma once
#include "Core/Panel.h"
#include "BasicActorDefs.h"
#include "raylib.h"
#include <functional>
#include <unordered_map>

class PlaceActorsPanel : public Panel {
public:
    ~PlaceActorsPanel();

    void Initialize() override;
    void Render() override;

    std::function<void(const BasicActorDef&, Vector2 dropPos)> onActorDragged;

private:
    void LoadIcons();
    void UnloadIcons();
    Texture2D GetIconForDef(const BasicActorDef& def) const;

    bool dragging = false;
    int dragIndex = -1;
    Vector2 dragPressPos{0, 0};

    bool iconsLoaded = false;
    std::unordered_map<std::string, Texture2D> icons;
};
