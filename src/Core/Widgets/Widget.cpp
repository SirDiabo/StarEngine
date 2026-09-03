#include "Widget.h"
#include "WidgetElement.h"

Widget::~Widget() {
    delete rootElement;
}

void Widget::SetSelection(WidgetElement* element) {
    if (currentSelection == element) return;
    if (currentSelection) {
        currentSelection->interactionState = WidgetInteractionState::Default;
        currentSelection->OnDeselected();
    }
    currentSelection = element;
    if (currentSelection) {
        currentSelection->interactionState = WidgetInteractionState::Selected;
        currentSelection->OnSelected();
    }
}

void Widget::Draw(Rectangle screenRect, float bufferScale) {
    if (!rootElement || !rootElement->visible) return;
    float scale = uiScale * bufferScale;
    Rectangle resolvedRootRect = rootElement->ResolveRect(screenRect, scale);
    rootElement->Draw(resolvedRootRect, scale);
}

void Widget::Tick(float deltaTime, Rectangle screenRect, float bufferScale) {
    if (!rootElement || !rootElement->visible) return;
    float scale = uiScale * bufferScale;
    Rectangle resolvedRootRect = rootElement->ResolveRect(screenRect, scale);
    rootElement->Tick(deltaTime, resolvedRootRect, scale);
}
