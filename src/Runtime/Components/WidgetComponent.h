#pragma once
#include "SceneComponent.h"
#include "Core/Widgets/Widget.h"

class WidgetComponent : public SceneComponent {
public:
    ~WidgetComponent() override;

    const char* GetClassName() const override { return "WidgetComponent"; }
    std::string GetObjectKind() const override { return "WidgetComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;

    WidgetRenderSpace renderSpace = WidgetRenderSpace::Screen;
    WidgetBillboardMode billboardMode = WidgetBillboardMode::YawOnly;
    bool forceNativeResolution = false;
    float uiScale = 1.0f;
    std::string inputDevice = "Unclaimed";
    std::string widgetAssetPath;

    Widget* GetWidget() const { return widget; }
    void SetWidget(Widget* newWidget);

private:
    Widget* widget = nullptr;
    std::string loadedWidgetAssetPath;
    void RebuildWidgetFromAsset();
};
