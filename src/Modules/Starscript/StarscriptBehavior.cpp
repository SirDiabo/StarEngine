#include "StarscriptBehavior.h"
#include "Runtime/PrintMessageLog.h"
#include "raylib.h"

namespace Starscript {

StarscriptBehavior::StarscriptBehavior(std::shared_ptr<Program> programToRun)
    : program(programToRun), instance(programToRun) {}

void StarscriptBehavior::ReportCompileErrors() {
    if (reportedCompileErrors) return;
    reportedCompileErrors = true;
    if (!program) return;
    for (const Diagnostic& d : program->diagnostics) {
        std::string text = "Starscript compile error (line " + std::to_string(d.line) + "): " + d.message;
        TraceLog(LOG_ERROR, "%s", text.c_str());
        PrintMessageLog::Get().Add(PrintMessageType::Error, text, 8.0f);
    }
}

void StarscriptBehavior::OnBeginPlay(Actor* owner) {
    instance.SetOwner(owner);
    if (!program || !program->valid) { ReportCompileErrors(); return; }
    instance.RunBeginPlay();
}

void StarscriptBehavior::OnTick(Actor* owner, float deltaTime) {
    instance.SetOwner(owner);
    instance.RunTick(deltaTime);
}

void StarscriptBehavior::OnEndPlay(Actor* owner) {
    instance.SetOwner(owner);
    instance.RunEndPlay();
}

void StarscriptBehavior::OnDestroyed(Actor* owner) {
    instance.SetOwner(owner);
    instance.RunDestroyed();
}

void StarscriptBehavior::OnCustomEvent(Actor* owner, const std::string& eventName,
                                       const std::vector<std::pair<std::string, std::string>>& params) {
    instance.SetOwner(owner);
    instance.RunCustomEvent(eventName, params);
}

void StarscriptBehavior::OnComponentEvent(Actor* owner, Component* component, const std::string& eventName,
                                          const std::vector<std::pair<std::string, std::string>>& params) {
    instance.SetOwner(owner);
    instance.RunComponentEvent(component, eventName, params);
}

void StarscriptBehavior::CollectSavableVariables(std::vector<SavableVariable>& out) {
    if (!program || !program->valid) return;
    for (size_t n = 0; n < program->vars.size(); n++) {
        const VarDef& def = *program->vars[n];
        if (def.saveFile.empty() || n >= instance.GlobalCount()) continue;
        const Value& value = instance.GlobalAt((int)n);
        SavableVariable saved;
        saved.name = def.name;
        saved.saveFile = def.saveFile;
        if (def.type.kind == Kind::Array) {
            saved.isArray = true;
            if (value.arr) for (const Value& e : *value.arr) saved.arrayValues.push_back(Serialize(e));
        } else {
            saved.value = Serialize(value);
        }
        out.push_back(std::move(saved));
    }
}

void StarscriptBehavior::ApplySavedVariable(const SavableVariable& saved) {
    int index = instance.FindVariable(saved.name);
    if (index < 0 || (size_t)index >= instance.GlobalCount()) return;
    const VarDef& def = *program->vars[(size_t)index];
    if (def.saveFile.empty()) return;
    if (def.type.kind == Kind::Array) {
        Value arr = Value::Array();
        for (const std::string& text : saved.arrayValues) arr.arr->push_back(Deserialize(MakeType(def.type.elem), text));
        instance.GlobalAt(index) = arr;
    } else {
        instance.GlobalAt(index) = Deserialize(def.type, saved.value);
    }
}

void StarscriptBehavior::VisitReplicatedVariables(const ReplicatedVariableVisitor& visitor) {
    if (!program || !program->valid) return;
    for (size_t n = 0; n < program->vars.size() && n < instance.GlobalCount(); n++) {
        const VarDef& def = *program->vars[n];
        if (def.replication == ReplicationMode::Local) continue;
        visitor(def.name, Serialize(instance.GlobalAt((int)n)), def.replication);
    }
}

bool StarscriptBehavior::FindReplicatedVariable(const std::string& name, ReplicationMode& outMode) {
    int index = instance.FindVariable(name);
    if (index < 0) return false;
    outMode = program->vars[(size_t)index]->replication;
    return true;
}

void StarscriptBehavior::SetReplicatedVariable(const std::string& name, const std::string& value) {
    int index = instance.FindVariable(name);
    if (index < 0 || (size_t)index >= instance.GlobalCount()) return;
    instance.GlobalAt(index) = Deserialize(program->vars[(size_t)index]->type, value);
}

bool StarscriptBehavior::FindEventReplication(const std::string& eventName, ReplicationMode& outMode) {
    return instance.FindCustomEvent(eventName, outMode);
}

}
