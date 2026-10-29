#include "StarscriptWidgetBehavior.h"
#include "StarscriptCompiler.h"
#include "Runtime/PrintMessageLog.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetElement.h"
#include "raylib.h"

namespace Starscript {

namespace {

constexpr int kCrossCallDepthLimit = 64;
int crossCallDepth = 0;

struct CrossCallScope {
    CrossCallScope() { crossCallDepth++; }
    ~CrossCallScope() { crossCallDepth--; }
};

void CollectElements(WidgetElement* element, std::vector<WidgetElementInfo>& out) {
    if (!element) return;
    out.push_back({ element->elementId, element->GetClassName() });
    for (WidgetElement* child : element->GetAuthoredChildren()) CollectElements(child, out);
}

}

StarscriptWidgetBehavior::StarscriptWidgetBehavior(std::string scriptSource, std::shared_ptr<WidgetCompileCache> sharedCache)
    : source(std::move(scriptSource)), cache(std::move(sharedCache)) {}

void StarscriptWidgetBehavior::ReportCompileErrors() {
    if (reportedCompileErrors || !program) return;
    reportedCompileErrors = true;
    for (const Diagnostic& d : program->diagnostics) {
        std::string text = "Starscript widget compile error (line " + std::to_string(d.line) + "): " + d.message;
        TraceLog(LOG_ERROR, "%s", text.c_str());
        PrintMessageLog::Get().Add(PrintMessageType::Error, text, 8.0f);
    }
}

void StarscriptWidgetBehavior::OnConstruct(Widget* owner) {
    std::vector<WidgetElementInfo> elements;
    if (owner) CollectElements(owner->rootElement, elements);

    std::string signature;
    for (const WidgetElementInfo& el : elements) signature += el.id + ":" + el.className + ";";

    if (cache && cache->program && cache->source == source && cache->signature == signature) {
        program = cache->program;
    } else {
        program = CompileWidget(source, elements);
        if (cache) {
            cache->source = source;
            cache->signature = signature;
            cache->program = program;
        }
    }

    instance = std::make_unique<Instance>(program);
    instance->SetWidget(owner);
    if (!program->valid) { ReportCompileErrors(); return; }
    instance->RunBeginPlay();
}

void StarscriptWidgetBehavior::OnTick(Widget* owner, float deltaTime) {
    if (!Ready()) return;
    instance->SetWidget(owner);
    instance->RunTick(deltaTime);
}

void StarscriptWidgetBehavior::OnDestruct(Widget* owner) {
    if (!Ready()) return;
    instance->SetWidget(owner);
    instance->RunEndPlay();
    instance->SetWidget(nullptr);
}

void StarscriptWidgetBehavior::OnElementEvent(Widget* owner, WidgetElement* target, const std::string& eventName) {
    if (!Ready() || !target) return;
    instance->SetWidget(owner);
    instance->RunElementEvent(target, eventName);
}

bool StarscriptWidgetBehavior::CallWidgetEvent(Widget* owner, const std::string& eventName, const CallParams& params) {
    if (!Ready() || crossCallDepth >= kCrossCallDepthLimit) return false;
    CrossCallScope scope;
    instance->SetWidget(owner);
    return instance->RunCustomEvent(eventName, params);
}

bool StarscriptWidgetBehavior::CallWidgetFunction(Widget* owner, const std::string& functionName, const CallParams& args, std::string& result) {
    if (!Ready() || crossCallDepth >= kCrossCallDepthLimit) return false;
    CrossCallScope scope;
    instance->SetWidget(owner);
    return instance->CallFunctionByName(functionName, args, result);
}

bool StarscriptWidgetBehavior::SetWidgetVariable(Widget*, const std::string& name, const std::string& value) {
    if (!Ready()) return false;
    int index = instance->FindVariable(name);
    if (index < 0 || (size_t)index >= instance->GlobalCount()) return false;
    instance->GlobalAt(index) = Deserialize(program->vars[(size_t)index]->type, value);
    return true;
}

bool StarscriptWidgetBehavior::GetWidgetVariable(Widget*, const std::string& name, std::string& value) {
    if (!Ready()) return false;
    int index = instance->FindVariable(name);
    if (index < 0 || (size_t)index >= instance->GlobalCount()) return false;
    value = Serialize(instance->GlobalAt(index));
    return true;
}

}
