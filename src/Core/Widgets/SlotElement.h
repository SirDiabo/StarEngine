#pragma once
#include "WidgetElement.h"

class SlotElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "SlotElement"; }
    std::string GetObjectKind() const override { return "SlotElement"; }

    std::string slotName = "Slot";

    static bool showPlaceholders;

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
    Rectangle ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const override;
};
