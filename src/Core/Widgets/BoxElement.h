#pragma once
#include "WidgetElement.h"

enum class BoxAxis {
    Vertical,
    Horizontal
};

class BoxElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "BoxElement"; }
    std::string GetObjectKind() const override { return "BoxElement"; }

    BoxAxis axis = BoxAxis::Vertical;
    bool scrollEnabled = false;
    float scrollOffset = 0.0f;
    float childSpacing = 0.0f;
    Color backgroundColor{ 0, 0, 0, 0 };

    void Draw(Rectangle rect, float uiScale) override;
    float GetContentExtent(float uiScale) const;

protected:
    Rectangle ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const override;
};
