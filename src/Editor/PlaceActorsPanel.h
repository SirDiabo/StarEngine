#pragma once
#include "Core/Panel.h"
#include "BasicActorDefs.h"
#include "raylib.h"
#include <functional>

class PlaceActorsPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

    std::function<void(const BasicActorDef&, Vector2 dropPos)> onActorDragged;

private:
    bool dragging = false;
    int dragIndex = -1;
    Vector2 dragPressPos{0, 0};
};
