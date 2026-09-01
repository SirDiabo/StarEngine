#pragma once
#include "WidgetElement.h"

class RectangleElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "RectangleElement"; }
    std::string GetObjectKind() const override { return "RectangleElement"; }

    Color color{ 255, 255, 255, 255 };

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
};
