#include "WidgetComponent.h"

WidgetComponent::~WidgetComponent() {
    delete widget;
}

void WidgetComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddBool("Force Native Resolution", &forceNativeResolution);
    outProperties.AddFloat("UI Scale", &uiScale);
    outProperties.AddString("Input Device", &inputDevice);
}

void WidgetComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
    if (widget) {
        widget->uiScale = uiScale;
        widget->forceNativeResolution = forceNativeResolution;
    }
}

void WidgetComponent::SetWidget(Widget* newWidget) {
    if (widget == newWidget) return;
    delete widget;
    widget = newWidget;
    if (widget) {
        widget->uiScale = uiScale;
        widget->forceNativeResolution = forceNativeResolution;
    }
}
