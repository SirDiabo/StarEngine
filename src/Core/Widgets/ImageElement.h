#pragma once
#include "WidgetElement.h"
#include <string>

class ImageElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "ImageElement"; }
    std::string GetObjectKind() const override { return "ImageElement"; }

    std::string texturePath;
    const Texture2D* directTexture = nullptr;
    Color tint{ 255, 255, 255, 255 };

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
};
