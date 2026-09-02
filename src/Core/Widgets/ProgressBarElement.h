#pragma once
#include "WidgetElement.h"

class ProgressBarElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "ProgressBarElement"; }
    std::string GetObjectKind() const override { return "ProgressBarElement"; }

    float value = 1.0f;
    Color backgroundColor{ 40, 40, 40, 255 };
    Color fillColor{ 90, 180, 90, 255 };

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
};
