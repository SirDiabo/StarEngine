#pragma once
#include "WidgetElement.h"
#include <functional>
#include <vector>

class SliderElement : public WidgetElement {
public:
    SliderElement() { selectable = true; }

    const char* GetClassName() const override { return "SliderElement"; }
    std::string GetObjectKind() const override { return "SliderElement"; }

    float value = 0.0f;
    float minValue = 0.0f;
    float maxValue = 1.0f;

    Color trackColor{ 45, 45, 50, 255 };
    Color fillColor{ 90, 140, 220, 255 };
    Color handleColor{ 230, 230, 235, 255 };
    Color borderColor{ 80, 80, 85, 255 };

    std::vector<Color> gradientStops;

    std::function<void(float)> onValueChanged;

    void OnConfirmed() override;

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
    void OnTickSelf(float deltaTime, Rectangle rect, float uiScale) override;

private:
    void ApplyMouseX(float mouseX, Rectangle rect);
    bool dragging = false;
};
