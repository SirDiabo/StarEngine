#pragma once
#include "Runtime/Object.h"
#include "raylib.h"
#include <string>

class WidgetElement;

enum class WidgetRenderSpace {
    Screen,
    World,
    Billboard
};

enum class WidgetBillboardMode {
    YawOnly,
    FullSpherical
};

class Widget : public Object {
public:
    ~Widget() override;

    const char* GetClassName() const override { return "Widget"; }
    std::string GetObjectKind() const override { return "Widget"; }

    WidgetElement* rootElement = nullptr;
    float uiScale = 1.0f;
    bool forceNativeResolution = false;

    WidgetElement* currentSelection = nullptr;
    float navigateCooldown = 0.0f;
    bool confirmWasHeld = false;
    bool cancelWasHeld = false;

    void SetSelection(WidgetElement* element);
    void Draw(Rectangle screenRect);
    void Tick(float deltaTime, Rectangle screenRect);
};
