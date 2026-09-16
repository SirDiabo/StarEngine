#pragma once
#include "WidgetElement.h"

class RectangleElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "RectangleElement"; }
    std::string GetObjectKind() const override { return "RectangleElement"; }

    Color color{ 255, 255, 255, 255 };
    float cornerRadius = 0.0f;
    float borderThickness = 0.0f;
    Color borderColor{ 0, 0, 0, 0 };

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
};
