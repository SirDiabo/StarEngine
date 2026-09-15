#include "CheckBoxElement.h"

void CheckBoxElement::OnConfirmed() {
    checked = !checked;
    if (onToggled) onToggled(checked);
    WidgetElement::OnConfirmed();
}

void CheckBoxElement::OnDrawSelf(Rectangle rect, float uiScale) {
    bool hovered = interactionState == WidgetInteractionState::Hovered || interactionState == WidgetInteractionState::Pressed;
    DrawRectangleRec(rect, hovered ? hoveredBoxColor : boxColor);
    DrawRectangleLinesEx(rect, 1.0f, borderColor);
    if (checked) {
        float pad = rect.width * 0.22f;
        Rectangle inner = { rect.x + pad, rect.y + pad, rect.width - pad * 2.0f, rect.height - pad * 2.0f };
        DrawRectangleRec(inner, checkColor);
    }
}
