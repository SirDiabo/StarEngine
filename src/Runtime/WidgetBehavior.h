#pragma once
#include <string>

class Widget;
class WidgetElement;

class WidgetBehavior {
public:
    virtual ~WidgetBehavior() = default;

    virtual void OnConstruct(Widget* owner) = 0;
    virtual void OnTick(Widget* owner, float deltaTime) = 0;
    virtual void OnDestruct(Widget* owner) = 0;

    virtual void OnElementEvent(Widget* owner, WidgetElement* target, const std::string& eventName) { }
};
