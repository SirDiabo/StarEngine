#include "WidgetComponent.h"
#include "Modules/Stargraph/WidgetAsset.h"

WidgetComponent::~WidgetComponent() {
    delete widget;
}

void WidgetComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddString("Widget Asset", &widgetAssetPath);
    outProperties.AddBool("Force Native Resolution", &forceNativeResolution);
    outProperties.AddFloat("UI Scale", &uiScale);
    outProperties.AddString("Input Device", &inputDevice);
}

void WidgetComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
    if (widgetAssetPath != loadedWidgetAssetPath) RebuildWidgetFromAsset();
    if (widget) {
        widget->uiScale = uiScale;
        widget->forceNativeResolution = forceNativeResolution;
    }
}

void WidgetComponent::RebuildWidgetFromAsset() {
    loadedWidgetAssetPath = widgetAssetPath;
    Widget* newWidget = widgetAssetPath.empty() ? nullptr : WidgetAsset::InstantiateWidget(widgetAssetPath);
    if (newWidget) {
        uiScale = newWidget->uiScale;
        forceNativeResolution = newWidget->forceNativeResolution;
    }
    SetWidget(newWidget);
}

void WidgetComponent::SetWidget(Widget* newWidget) {
    if (widget == newWidget) return;
    delete widget;
    widget = newWidget;
    if (widget) {
        widget->uiScale = uiScale;
        widget->forceNativeResolution = forceNativeResolution;
        widget->owningComponent = this;
    }
}
