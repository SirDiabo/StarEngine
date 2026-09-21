#pragma once
#include "Runtime/Object.h"
#include "raylib.h"
#include <string>

class WidgetElement;
class WidgetBehavior;
class WidgetComponent;

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
    bool hadKeyboardFocus = false;
    std::string pendingSelectionKey;

    WidgetComponent* owningComponent = nullptr;

    void SetSelection(WidgetElement* element);
    void SetRoot(WidgetElement* root);
    void Draw(Rectangle screenRect, float bufferScale = 1.0f);
    void Tick(float deltaTime, Rectangle screenRect, float bufferScale = 1.0f);

    WidgetBehavior* GetBehavior() const { return behavior; }
    void SetBehavior(WidgetBehavior* newBehavior);
    void FireElementEvent(WidgetElement* target, const std::string& eventName);

private:
    WidgetBehavior* behavior = nullptr;
};
