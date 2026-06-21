#pragma once
#include "Core/Panel.h"
#include "raylib.h"

class ViewportPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

private:
    Camera3D camera;
};