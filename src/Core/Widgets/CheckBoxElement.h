#pragma once
#include "WidgetElement.h"
#include <functional>

class CheckBoxElement : public WidgetElement {
public:
    CheckBoxElement() { selectable = true; }

    const char* GetClassName() const override { return "CheckBoxElement"; }
    std::string GetObjectKind() const override { return "CheckBoxElement"; }

    bool checked = false;
    Color boxColor{ 40, 40, 45, 255 };
    Color hoveredBoxColor{ 55, 55, 60, 255 };
    Color borderColor{ 90, 90, 95, 255 };
    Color checkColor{ 90, 140, 220, 255 };

    std::function<void(bool)> onToggled;

    void OnConfirmed() override;

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
};
