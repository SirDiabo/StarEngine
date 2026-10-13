#include "WidgetComponent.h"
#include "Core/Widgets/WidgetAsset.h"
#include "Modules/Stargraph/ComponentRegistry.h"
#include "Core/SafeParse.h"

WidgetComponent::~WidgetComponent() {
    delete widget;
    if (worldTextureValid) UnloadRenderTexture(worldTexture);
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

RenderTexture2D& WidgetComponent::GetOrCreateWorldTexture(int width, int height) {
    if (!worldTextureValid || worldTextureWidth != width || worldTextureHeight != height) {
        if (worldTextureValid) UnloadRenderTexture(worldTexture);
        worldTexture = LoadRenderTexture(width, height);
        worldTextureWidth = width;
        worldTextureHeight = height;
        worldTextureValid = true;
    }
    return worldTexture;
}

namespace {
ComponentRegistrar registrar_WidgetComponent({
    "WidgetComponent",
    []() -> Component* { return new WidgetComponent(); },
    {
        ComponentField::Asset("WidgetAsset", "Widget Asset", ".starwidget",
            [](Component& c, const ComponentDecl& d) { static_cast<WidgetComponent&>(c).widgetAssetPath = d.Get("WidgetAsset"); }, [](const Component& c) { return static_cast<const WidgetComponent&>(c).widgetAssetPath; }),
        ComponentField::Bln("WidgetForceNativeResolution", "Force Native Resolution", false,
            [](Component& c, const ComponentDecl& d) { static_cast<WidgetComponent&>(c).forceNativeResolution = d.Get("WidgetForceNativeResolution", "false") == "true"; }, [](const Component& c) { return ComponentField::BoolStr(static_cast<const WidgetComponent&>(c).forceNativeResolution); }),
        ComponentField::Flt("WidgetUIScale", "UI Scale", 1.0f,
            [](Component& c, const ComponentDecl& d) { static_cast<WidgetComponent&>(c).uiScale = SafeParse::ToFloat(d.Get("WidgetUIScale", "1")); }, [](const Component& c) { return ComponentField::FloatStr(static_cast<const WidgetComponent&>(c).uiScale); }),
        ComponentField::Dev("InputDevice", "Device",
            [](Component& c, const ComponentDecl& d) { static_cast<WidgetComponent&>(c).inputDevice = d.Get("InputDevice", "Unclaimed"); }, [](const Component& c) { return static_cast<const WidgetComponent&>(c).inputDevice; }),
        ComponentField::Enm("WidgetRenderSpace", "Render Space", {"Screen", "World", "Billboard"}, "Screen",
            [](Component& c, const ComponentDecl& d) {
                std::string v = d.Get("WidgetRenderSpace", "Screen");
                static_cast<WidgetComponent&>(c).renderSpace = v == "World" ? WidgetRenderSpace::World
                                                             : v == "Billboard" ? WidgetRenderSpace::Billboard
                                                                                 : WidgetRenderSpace::Screen;
            },
            [](const Component& c) -> std::string {
                switch (static_cast<const WidgetComponent&>(c).renderSpace) {
                    case WidgetRenderSpace::World:     return "World";
                    case WidgetRenderSpace::Billboard: return "Billboard";
                    default:                           return "Screen";
                }
            }),
        [] { ComponentFieldDesc f = ComponentField::Enm("WidgetBillboardMode", "Billboard Mode", {"YawOnly", "FullSpherical"}, "YawOnly",
                [](Component& c, const ComponentDecl& d) {
                    static_cast<WidgetComponent&>(c).billboardMode = d.Get("WidgetBillboardMode", "YawOnly") == "FullSpherical"
                        ? WidgetBillboardMode::FullSpherical : WidgetBillboardMode::YawOnly;
                });
             f.read = [](const Component& c) -> std::string {
                 return static_cast<const WidgetComponent&>(c).billboardMode == WidgetBillboardMode::FullSpherical ? "FullSpherical" : "YawOnly";
             };
             f.visibleIf = [](const ComponentDecl& d) { return d.Get("WidgetRenderSpace", "Screen") == "Billboard"; };
             return f; }(),
    }
});
}
