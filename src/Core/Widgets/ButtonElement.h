#pragma once
#include "RectangleElement.h"

class ButtonElement : public RectangleElement {
public:
    ButtonElement() { selectable = true; }

    const char* GetClassName() const override { return "ButtonElement"; }
    bool RaisesPointerEvents() const override { return true; }
    std::string GetObjectKind() const override { return "ButtonElement"; }

    Color defaultColor{ 70, 70, 75, 255 };
    Color hoveredColor{ 95, 95, 100, 255 };
    Color selectedColor{ 90, 140, 220, 255 };
    Color pressedColor{ 60, 100, 170, 255 };
    Color disabledColor{ 45, 45, 45, 255 };

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
};
