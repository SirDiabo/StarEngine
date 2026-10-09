#include "Widget.h"
#include "Core/Profiler.h"
#include "WidgetElement.h"
#include "Runtime/WidgetBehavior.h"
#include "WidgetInput.h"
#include "raylib.h"

Widget::~Widget() {
    WidgetInput::Forget(this);
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
    static float lastLoggedScale = -1.0f;
    if (scale != lastLoggedScale) {
        TraceLog(LOG_INFO, "Widget::Draw uiScale=%.3f bufferScale=%.3f scale=%.3f", uiScale, bufferScale, scale);
        lastLoggedScale = scale;
    }
    PROFILE_SCOPE("Element Draw");
    rootElement->Draw(screenRect, scale);
}

void Widget::Tick(float deltaTime, Rectangle screenRect, float bufferScale) {
    if (behavior) {
        PROFILE_SCOPE("Widget Graph Tick");
        behavior->OnTick(this, deltaTime);
    }
    if (!rootElement || !rootElement->visible) return;
    float scale = uiScale * bufferScale;
    PROFILE_SCOPE("Element Tick");
    rootElement->Tick(deltaTime, screenRect, scale);
}
