#include "SplitBoxElement.h"

void SplitBoxElement::Setup(WidgetElement* first, WidgetElement* second) {
    AddChild(first);
    AddChild(second);
}

void SplitBoxElement::Draw(Rectangle rect, float uiScale) {
    const auto& kids = GetChildren();
    if (kids.size() < 2) return;

    float thickness = dividerThickness * uiScale;
    if (axis == BoxAxis::Horizontal) {
        float firstWidth = (rect.width - thickness) * splitRatio;
        Rectangle firstRect{ rect.x, rect.y, firstWidth, rect.height };
        Rectangle dividerRect{ rect.x + firstWidth, rect.y, thickness, rect.height };
        Rectangle secondRect{ rect.x + firstWidth + thickness, rect.y, rect.width - firstWidth - thickness, rect.height };
        kids[0]->Draw(kids[0]->FitToSlot(firstRect, uiScale), uiScale);
        DrawRectangleRec(dividerRect, dividerColor);
        kids[1]->Draw(kids[1]->FitToSlot(secondRect, uiScale), uiScale);
    } else {
        float firstHeight = (rect.height - thickness) * splitRatio;
        Rectangle firstRect{ rect.x, rect.y, rect.width, firstHeight };
        Rectangle dividerRect{ rect.x, rect.y + firstHeight, rect.width, thickness };
        Rectangle secondRect{ rect.x, rect.y + firstHeight + thickness, rect.width, rect.height - firstHeight - thickness };
        kids[0]->Draw(kids[0]->FitToSlot(firstRect, uiScale), uiScale);
        DrawRectangleRec(dividerRect, dividerColor);
        kids[1]->Draw(kids[1]->FitToSlot(secondRect, uiScale), uiScale);
    }
}

void SplitBoxElement::Tick(float deltaTime, Rectangle rect, float uiScale) {
    const auto& kids = GetChildren();
    if (kids.size() < 2) return;

    float thickness = dividerThickness * uiScale;
    if (axis == BoxAxis::Horizontal) {
        float firstWidth = (rect.width - thickness) * splitRatio;
        Rectangle firstRect{ rect.x, rect.y, firstWidth, rect.height };
        Rectangle secondRect{ rect.x + firstWidth + thickness, rect.y, rect.width - firstWidth - thickness, rect.height };
        kids[0]->Tick(deltaTime, kids[0]->FitToSlot(firstRect, uiScale), uiScale);
        kids[1]->Tick(deltaTime, kids[1]->FitToSlot(secondRect, uiScale), uiScale);
    } else {
        float firstHeight = (rect.height - thickness) * splitRatio;
        Rectangle firstRect{ rect.x, rect.y, rect.width, firstHeight };
        Rectangle secondRect{ rect.x, rect.y + firstHeight + thickness, rect.width, rect.height - firstHeight - thickness };
        kids[0]->Tick(deltaTime, kids[0]->FitToSlot(firstRect, uiScale), uiScale);
        kids[1]->Tick(deltaTime, kids[1]->FitToSlot(secondRect, uiScale), uiScale);
    }
}
