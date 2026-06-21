#pragma once
#include "Core/Panel.h"

class ViewportPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~ViewportPanel() override;

private:
    void EnsureRenderTarget(int width, int height);

    Camera3D camera{};
    RenderTexture2D renderTarget{};
    int renderTargetWidth = 0;
    int renderTargetHeight = 0;
};
