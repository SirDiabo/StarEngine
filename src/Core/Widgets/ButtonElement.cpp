#include "ButtonElement.h"

void ButtonElement::OnDrawSelf(Rectangle rect, float uiScale) {
    switch (interactionState) {
        case WidgetInteractionState::Disabled: color = disabledColor; break;
        case WidgetInteractionState::Pressed:  color = pressedColor;  break;
        case WidgetInteractionState::Selected: color = selectedColor; break;
        case WidgetInteractionState::Hovered:  color = hoveredColor;  break;
        default:                               color = defaultColor; break;
    }
    RectangleElement::OnDrawSelf(rect, uiScale);
}
