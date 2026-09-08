#include "WidgetComponent.h"
#include "Modules/Stargraph/WidgetAsset.h"
#include "Modules/Stargraph/ComponentRegistry.h"
#include "Core/SafeParse.h"

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

namespace {
ComponentRegistrar registrar_WidgetComponent({
    "WidgetComponent",
    []() -> Component* { return new WidgetComponent(); },
    {
        ComponentField::Asset("WidgetAsset", "Widget Asset", ".starwidget",
            [](Component& c, const ComponentDecl& d) { static_cast<WidgetComponent&>(c).widgetAssetPath = d.Get("WidgetAsset"); }),
        ComponentField::Bln("WidgetForceNativeResolution", "Force Native Resolution", false,
            [](Component& c, const ComponentDecl& d) { static_cast<WidgetComponent&>(c).forceNativeResolution = d.Get("WidgetForceNativeResolution", "false") == "true"; }),
        ComponentField::Flt("WidgetUIScale", "UI Scale", 1.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<WidgetComponent&>(c).uiScale = SafeParse::ToFloat(d.Get("WidgetUIScale", "1")); }),
        ComponentField::Dev("InputDevice", "Device",
            [](Component& c, const ComponentDecl& d) { static_cast<WidgetComponent&>(c).inputDevice = d.Get("InputDevice", "Unclaimed"); }),
        ComponentField::Enm("WidgetRenderSpace", "Render Space", {"Screen", "World", "Billboard"}, "Screen",
            [](Component& c, const ComponentDecl& d) {
                std::string v = d.Get("WidgetRenderSpace", "Screen");
                static_cast<WidgetComponent&>(c).renderSpace = v == "World" ? WidgetRenderSpace::World
                                                             : v == "Billboard" ? WidgetRenderSpace::Billboard
                                                                                 : WidgetRenderSpace::Screen;
            }),
        [] { ComponentFieldDesc f = ComponentField::Enm("WidgetBillboardMode", "Billboard Mode", {"YawOnly", "FullSpherical"}, "YawOnly",
                [](Component& c, const ComponentDecl& d) {
                    static_cast<WidgetComponent&>(c).billboardMode = d.Get("WidgetBillboardMode", "YawOnly") == "FullSpherical"
                        ? WidgetBillboardMode::FullSpherical : WidgetBillboardMode::YawOnly;
                });
             f.visibleIf = [](const ComponentDecl& d) { return d.Get("WidgetRenderSpace", "Screen") == "Billboard"; };
             return f; }(),
    }
});
}
