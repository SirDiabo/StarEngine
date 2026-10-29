#pragma once
#include <string>
#include <utility>
#include <vector>

class Widget;
class WidgetElement;

class WidgetBehavior {
public:
    virtual ~WidgetBehavior() = default;

    virtual void OnConstruct(Widget* owner) = 0;
    virtual void OnTick(Widget* owner, float deltaTime) = 0;
    virtual void OnDestruct(Widget* owner) = 0;

    virtual void OnElementEvent(Widget* owner, WidgetElement* target, const std::string& eventName) { }

    using CallParams = std::vector<std::pair<std::string, std::string>>;
    virtual bool CallWidgetEvent(Widget* owner, const std::string& eventName, const CallParams& params) { return false; }
    virtual bool CallWidgetFunction(Widget* owner, const std::string& functionName, const CallParams& args, std::string& result) { return false; }
    virtual bool SetWidgetVariable(Widget* owner, const std::string& name, const std::string& value) { return false; }
    virtual bool GetWidgetVariable(Widget* owner, const std::string& name, std::string& value) { return false; }
};
