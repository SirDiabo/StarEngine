#include "Widget.h"
#include "WidgetElement.h"
#include "Runtime/WidgetBehavior.h"

Widget::~Widget() {
    if (behavior) behavior->OnDestruct(this);
    delete behavior;
    delete rootElement;
}

void Widget::SetRoot(WidgetElement* root) {
    rootElement = root;
    if (root) root->SetOwningWidgetRecursive(this);
}

void Widget::SetBehavior(WidgetBehavior* newBehavior) {
    if (behavior == newBehavior) return;
    delete behavior;
    behavior = newBehavior;
    if (behavior) behavior->OnConstruct(this);
}

void Widget::FireElementEvent(WidgetElement* target, const std::string& eventName) {
    if (behavior) behavior->OnElementEvent(this, target, eventName);
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
    rootElement->Draw(screenRect, scale);
}

void Widget::Tick(float deltaTime, Rectangle screenRect, float bufferScale) {
    if (behavior) behavior->OnTick(this, deltaTime);
    if (!rootElement || !rootElement->visible) return;
    float scale = uiScale * bufferScale;
    rootElement->Tick(deltaTime, screenRect, scale);
}
