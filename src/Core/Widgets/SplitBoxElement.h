#pragma once
#include "BoxElement.h"

class SplitBoxElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "SplitBoxElement"; }
    std::string GetObjectKind() const override { return "SplitBoxElement"; }

    BoxAxis axis = BoxAxis::Horizontal;
    float splitRatio = 0.5f;
    float dividerThickness = 4.0f;
    Color dividerColor{ 60, 60, 65, 255 };

    void Setup(WidgetElement* first, WidgetElement* second);

    void Draw(Rectangle rect, float uiScale) override;
    void Tick(float deltaTime, Rectangle rect, float uiScale) override;
};
