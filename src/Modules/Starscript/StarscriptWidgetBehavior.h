#pragma once
#include "Runtime/WidgetBehavior.h"
#include "StarscriptRuntime.h"
#include "StarscriptInterface.h"
#include <memory>
#include <string>

namespace Starscript {

class StarscriptWidgetBehavior : public WidgetBehavior {
public:
    StarscriptWidgetBehavior(std::string source, std::shared_ptr<WidgetCompileCache> cache);

    void OnConstruct(Widget* owner) override;
    void OnTick(Widget* owner, float deltaTime) override;
    void OnDestruct(Widget* owner) override;
    void OnElementEvent(Widget* owner, WidgetElement* target, const std::string& eventName) override;

    bool CallWidgetEvent(Widget* owner, const std::string& eventName, const CallParams& params) override;
    bool CallWidgetFunction(Widget* owner, const std::string& functionName, const CallParams& args, std::string& result) override;
    bool SetWidgetVariable(Widget* owner, const std::string& name, const std::string& value) override;
    bool GetWidgetVariable(Widget* owner, const std::string& name, std::string& value) override;

    Instance* GetInstance() { return instance.get(); }

private:
    bool Ready() const { return instance && program && program->valid; }
    void ReportCompileErrors();

    std::string source;
    std::shared_ptr<WidgetCompileCache> cache;
    std::shared_ptr<Program> program;
    std::unique_ptr<Instance> instance;
    bool reportedCompileErrors = false;
};

}
