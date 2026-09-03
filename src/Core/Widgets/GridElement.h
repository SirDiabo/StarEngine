#pragma once
#include "WidgetElement.h"

class GridElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "GridElement"; }
    std::string GetObjectKind() const override { return "GridElement"; }

    int columns = 2;
    float columnWidth = 120.0f;
    float rowHeight = 40.0f;
    float cellSpacing = 8.0f;

    WidgetElement* GetNextSelectableInDirection(WidgetElement* current, int dx, int dy) const override;

protected:
    Rectangle ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const override;
};
