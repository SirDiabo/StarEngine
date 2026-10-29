#include "StarscriptRuntime.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetElement.h"
#include "StarscriptInput.h"
#include "Runtime/Actor.h"
#include "Runtime/PrintMessageLog.h"
#include "Runtime/Components/Component.h"
#include "Runtime/Networking/NetworkBackend.h"
#include "Core/ObjectRefRegistry.h"
#include "raylib.h"
#include <cmath>

namespace Starscript {

namespace {

constexpr int kStepLimit = 5000000;
constexpr int kDepthLimit = 200;

std::string SerializeArg(const Value& v) {
    if (v.kind == Kind::Actor || v.kind == Kind::Component || v.kind == Kind::Element) {
        if (v.objectId == 0) return std::string();
        if (Object* obj = ObjectRefRegistry::Get().Resolve(v.objectId))
            return ObjectRefRegistry::Encode(obj->GetObjectKind(), v.objectId);
        return std::string();
    }
    return Serialize(v);
}

std::string NormalizeParamName(const std::string& name) {
    std::string out;
    for (char c : name) {
        if (c == ' ' || c == '_') continue;
        out += (char)tolower((unsigned char)c);
    }
    return out;
}

}

Instance::Instance(std::shared_ptr<Program> programToRun) : program(std::move(programToRun)) {
    InitializeGlobals();
}

void Instance::ReportError(const std::string& message, int line) {
    if (!reportedErrors.insert({ line, message }).second) return;
    std::string text = "Starscript error (line " + std::to_string(line) + "): " + message;
    TraceLog(LOG_ERROR, "%s", text.c_str());
    PrintMessageLog::Get().Add(PrintMessageType::Error, text, 6.0f);
}

void Instance::Guarded(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const ScriptError& error) {
        ReportError(error.message, currentLine);
    }
}

void Instance::Step() {
    if (++stepCount > kStepLimit) throw ScriptError{ "Script exceeded the execution limit (possible infinite loop)" };
}

Value Instance::Coerce(const SType& target, Value value) const {
    if (target.kind == Kind::Float && value.kind == Kind::Int) return Value::Float((double)value.i);
    if (target.kind == Kind::Array && target.elem == Kind::Float && value.arr) {
        for (Value& e : *value.arr) if (e.kind == Kind::Int) e = Value::Float((double)e.i);
    }
    return value;
}

void Instance::InitializeGlobals() {
    globals.clear();
    if (!program || !program->valid) return;

    initializing = true;
    stepCount = 0;
    for (const auto& var : program->vars) {
        Value v = Value::Default(var->type);
        if (var->init) {
            Frame frame;
            try {
                currentLine = var->line;
                v = Coerce(var->type, Eval(*var->init, frame));
            } catch (const ScriptError& error) {
                ReportError(error.message, var->line);
            }
        }
        globals.push_back(std::move(v));
    }
    initializing = false;
}

int Instance::FindVariable(const std::string& name) const {
    if (!program) return -1;
    auto it = program->varIndex.find(name);
    return it == program->varIndex.end() ? -1 : it->second;
}

bool Instance::FindCustomEvent(const std::string& name, ReplicationMode& outMode) const {
    if (!program) return false;
    for (const auto& e : program->events) {
        if (e->kind == EventKind::Custom && e->name == name) { outMode = e->replication; return true; }
    }
    return false;
}

void Instance::RunEventBody(const EventDef& event, std::vector<Value>& args) {
    if (event.hasAwait) { StartCoroutine(event, args); return; }
    if (callDepth >= kDepthLimit) throw ScriptError{ "Call depth limit exceeded (runaway recursion?)" };
    Frame frame;
    frame.locals.resize((size_t)std::max(event.numLocals, (int)args.size()));
    for (size_t n = 0; n < args.size() && n < event.params.size(); n++) frame.locals[n] = Coerce(event.params[n].type, args[n]);
    callDepth++;
    try {
        ExecBlock(event.body, frame);
    } catch (...) {
        callDepth--;
        throw;
    }
    callDepth--;
}

Value Instance::CallFunction(const FuncDef& func, std::vector<Value>& args) {
    if (callDepth >= kDepthLimit) throw ScriptError{ "Call depth limit exceeded (runaway recursion?)" };
    Frame frame;
    frame.locals.resize((size_t)std::max(func.numLocals, (int)args.size()));
    for (size_t n = 0; n < args.size() && n < func.params.size(); n++) frame.locals[n] = Coerce(func.params[n].type, args[n]);
    callDepth++;
    try {
        ExecBlock(func.body, frame);
    } catch (...) {
        callDepth--;
        throw;
    }
    callDepth--;
    if (func.ret.kind == Kind::Void) return Value::Void();
    if (frame.returnValue.kind == Kind::Void) return Value::Default(func.ret);
    return Coerce(func.ret, frame.returnValue);
}

void Instance::StartCoroutine(const EventDef& event, std::vector<Value>& args) {
    auto co = std::make_unique<Coroutine>();
    co->frame.locals.resize((size_t)std::max(event.numLocals, (int)args.size()));
    for (size_t n = 0; n < args.size() && n < event.params.size(); n++) co->frame.locals[n] = Coerce(event.params[n].type, args[n]);
    Level level;
    level.type = Level::Type::Plain;
    level.block = &event.body;
    co->stack.push_back(level);
    if (!ResumeCoroutine(*co)) coroutines.push_back(std::move(co));
}

bool Instance::ResumeCoroutine(Coroutine& co) {
    Frame& frame = co.frame;
    co.wait = Coroutine::Wait::None;

    while (!co.stack.empty()) {
        {
            Level& lv = co.stack.back();
            if (lv.index >= lv.block->size()) {
                switch (lv.type) {
                    case Level::Type::Plain:
                        co.stack.pop_back();
                        break;
                    case Level::Type::While:
                        Step();
                        if (Eval(*lv.stmt->conds[0], frame).b) lv.index = 0;
                        else co.stack.pop_back();
                        break;
                    case Level::Type::ForRange: {
                        lv.cur += lv.step;
                        bool more = lv.step > 0 ? lv.cur < lv.stop : lv.cur > lv.stop;
                        if (more) { frame.locals[(size_t)lv.stmt->slot] = Value::Int(lv.cur); lv.index = 0; }
                        else co.stack.pop_back();
                        break;
                    }
                    case Level::Type::ForArray: {
                        lv.arrayIndex++;
                        if (lv.array.arr && lv.arrayIndex < lv.array.arr->size()) {
                            frame.locals[(size_t)lv.stmt->slot] = (*lv.array.arr)[lv.arrayIndex];
                            lv.index = 0;
                        } else co.stack.pop_back();
                        break;
                    }
                }
                continue;
            }
        }

        const Stmt& s = *(*co.stack.back().block)[co.stack.back().index++];
        Step();
        currentLine = s.line;

        switch (s.kind) {
            case StmtKind::VarDecl:
            case StmtKind::Assign:
            case StmtKind::ExprStmt:
            case StmtKind::Pass:
                ExecStmt(s, frame);
                break;
            case StmtKind::Await: {
                const Expr& call = *s.value;
                if (call.builtin && call.builtin->asyncKind == 1) {
                    double seconds = call.args.empty() ? 0.0 : Eval(*call.args[0], frame).AsNumber();
                    co.wait = Coroutine::Wait::Time;
                    co.remaining = (float)std::max(0.0, seconds);
                } else {
                    co.wait = Coroutine::Wait::NextFrame;
                }
                return false;
            }
            case StmtKind::If: {
                bool taken = false;
                for (size_t n = 0; n < s.conds.size() && !taken; n++) {
                    if (Eval(*s.conds[n], frame).b) {
                        Level level;
                        level.block = &s.blocks[n];
                        co.stack.push_back(level);
                        taken = true;
                    }
                }
                if (!taken && !s.elseBlock.empty()) {
                    Level level;
                    level.block = &s.elseBlock;
                    co.stack.push_back(level);
                }
                break;
            }
            case StmtKind::While: {
                if (Eval(*s.conds[0], frame).b) {
                    Level level;
                    level.type = Level::Type::While;
                    level.block = &s.body;
                    level.stmt = &s;
                    co.stack.push_back(level);
                }
                break;
            }
            case StmtKind::For: {
                const Expr& it = *s.iterable;
                Level level;
                level.block = &s.body;
                level.stmt = &s;
                if (s.isRange) {
                    long long start = 0, stop = 0, step = 1;
                    std::vector<long long> a;
                    for (const ExprPtr& arg : it.args) a.push_back(Eval(*arg, frame).i);
                    if (a.size() == 1) stop = a[0];
                    else if (a.size() >= 2) { start = a[0]; stop = a[1]; }
                    if (a.size() == 3) step = a[2];
                    if (step == 0) throw ScriptError{ "range() step cannot be 0" };
                    bool any = step > 0 ? start < stop : start > stop;
                    if (any) {
                        level.type = Level::Type::ForRange;
                        level.cur = start; level.stop = stop; level.step = step;
                        frame.locals[(size_t)s.slot] = Value::Int(start);
                        co.stack.push_back(level);
                    }
                } else {
                    Value arr = Eval(it, frame);
                    if (arr.arr && !arr.arr->empty()) {
                        level.type = Level::Type::ForArray;
                        level.array = arr;
                        level.arrayIndex = 0;
                        frame.locals[(size_t)s.slot] = (*arr.arr)[0];
                        co.stack.push_back(level);
                    }
                }
                break;
            }
            case StmtKind::Return:
                co.stack.clear();
                return true;
            case StmtKind::Break:
                while (!co.stack.empty()) {
                    Level::Type type = co.stack.back().type;
                    co.stack.pop_back();
                    if (type != Level::Type::Plain) break;
                }
                break;
            case StmtKind::Continue:
                while (!co.stack.empty() && co.stack.back().type == Level::Type::Plain) co.stack.pop_back();
                if (!co.stack.empty()) co.stack.back().index = co.stack.back().block->size();
                break;
        }
    }
    return true;
}

void Instance::UpdateCoroutines(float deltaTime) {
    if (coroutines.empty()) return;
    std::vector<std::unique_ptr<Coroutine>> current = std::move(coroutines);
    coroutines.clear();
    std::vector<std::unique_ptr<Coroutine>> survivors;
    for (auto& co : current) {
        bool ready = false;
        if (co->wait == Coroutine::Wait::NextFrame) ready = true;
        else if (co->wait == Coroutine::Wait::Time) { co->remaining -= deltaTime; ready = co->remaining <= 0.0f; }
        if (!ready) { survivors.push_back(std::move(co)); continue; }

        stepCount = 0;
        bool finished = true;
        try {
            finished = ResumeCoroutine(*co);
        } catch (const ScriptError& error) {
            ReportError(error.message, currentLine);
        }
        if (!finished) survivors.push_back(std::move(co));
    }
    for (auto& co : survivors) coroutines.push_back(std::move(co));
}

void Instance::CancelAsync() {
    coroutines.clear();
    timers.clear();
}

void Instance::SetTimer(const std::string& target, float interval, bool looping) {
    for (Timer& t : timers) {
        if (t.target == target) { t.interval = interval; t.remaining = interval; t.looping = looping; t.paused = false; return; }
    }
    Timer t;
    t.target = target;
    t.interval = interval;
    t.remaining = interval;
    t.looping = looping;
    timers.push_back(t);
}

void Instance::ClearTimer(const std::string& target) {
    for (size_t i = 0; i < timers.size(); i++) {
        if (timers[i].target == target) { timers.erase(timers.begin() + (long)i); return; }
    }
}

bool Instance::SetTimerPaused(const std::string& target, bool paused) {
    for (Timer& t : timers) if (t.target == target) { t.paused = paused; return true; }
    return false;
}

bool Instance::IsTimerActive(const std::string& target) const {
    for (const Timer& t : timers) if (t.target == target && !t.paused) return true;
    return false;
}

float Instance::TimerRemaining(const std::string& target) const {
    for (const Timer& t : timers) if (t.target == target) return std::max(0.0f, t.remaining);
    return 0.0f;
}

void Instance::FireTimerTarget(const std::string& target) {
    if (!program) return;
    stepCount = 0;
    ReplicationMode mode;
    if (FindCustomEvent(target, mode)) { RunCustomEvent(target, {}); return; }
    auto it = program->funcIndex.find(target);
    if (it != program->funcIndex.end()) {
        const FuncDef& f = *program->funcs[(size_t)it->second];
        if (f.params.empty()) {
            Guarded([&]() { std::vector<Value> args; CallFunction(f, args); });
        }
    }
}

const Instance::InputSample& Instance::QueryInput(const std::string& name) {
    InputSample& s = inputSamples[name];
    if (s.frame == inputFrame) return s;
    InputReading reading = SampleInput(owner, name, widget != nullptr);
    s.prevHeld = s.frame < 0 ? false : s.held;
    s.prevAxis = s.frame < 0 ? 0.0f : s.axis;
    s.held = reading.held;
    s.axis = reading.axis;
    s.x = reading.x;
    s.y = reading.y;
    s.frame = inputFrame;
    return s;
}

void Instance::RefreshInput() {
    inputFrame++;
    for (auto& kv : inputSamples) {
        InputSample& s = kv.second;
        InputReading reading = SampleInput(owner, kv.first, widget != nullptr);
        s.prevHeld = s.held;
        s.prevAxis = s.axis;
        s.held = reading.held;
        s.axis = reading.axis;
        s.x = reading.x;
        s.y = reading.y;
        s.frame = inputFrame;
    }
}

void Instance::DispatchInputEvents() {
    for (const auto& e : program->events) {
        if (e->kind != EventKind::Input) continue;
        const InputSample& s = QueryInput(e->inputName);
        bool fire = false;
        std::vector<Value> args;
        if (e->params.empty()) {
            fire = s.held && !s.prevHeld;
        } else if (e->params[0].type.kind == Kind::Bool) {
            fire = s.held != s.prevHeld;
            args.push_back(Value::Bool(s.held));
        } else {
            fire = s.axis != 0.0f || s.prevAxis != 0.0f;
            args.push_back(Value::Float(s.axis));
        }
        if (!fire) continue;
        stepCount = 0;
        Guarded([&]() { RunEventBody(*e, args); });
    }
}

void Instance::UpdateTimers(float deltaTime) {
    if (timers.empty()) return;
    std::vector<std::string> due;
    for (Timer& t : timers) {
        if (t.paused) continue;
        t.remaining -= deltaTime;
        if (t.remaining <= 0.0f) due.push_back(t.target);
    }
    for (const std::string& target : due) {
        size_t index = timers.size();
        for (size_t i = 0; i < timers.size(); i++) if (timers[i].target == target) { index = i; break; }
        if (index == timers.size()) continue;
        if (timers[index].looping && timers[index].interval > 0.0f) timers[index].remaining += timers[index].interval;
        else timers.erase(timers.begin() + (long)index);
        FireTimerTarget(target);
    }
}

void Instance::RunBeginPlay() {
    if (!program || !program->valid) return;
    stepCount = 0;
    for (const auto& e : program->events) {
        if (e->kind != EventKind::BeginPlay) continue;
        Guarded([&]() { std::vector<Value> args; RunEventBody(*e, args); });
    }
}

void Instance::RunTick(float deltaTime) {
    if (!program || !program->valid) return;
    RefreshInput();
    UpdateTimers(deltaTime);
    UpdateCoroutines(deltaTime);
    DispatchInputEvents();
    for (const auto& e : program->events) {
        if (e->kind != EventKind::Tick) continue;
        stepCount = 0;
        Guarded([&]() {
            std::vector<Value> args;
            if (!e->params.empty()) args.push_back(Value::Float(deltaTime));
            RunEventBody(*e, args);
        });
    }
}

void Instance::RunEndPlay() {
    if (!program || !program->valid) return;
    CancelAsync();
    stepCount = 0;
    for (const auto& e : program->events) {
        if (e->kind != EventKind::EndPlay) continue;
        Guarded([&]() { std::vector<Value> args; RunEventBody(*e, args); });
    }
}

void Instance::RunDestroyed() {
    if (!program || !program->valid) return;
    CancelAsync();
    stepCount = 0;
    for (const auto& e : program->events) {
        if (e->kind != EventKind::Destroyed) continue;
        Guarded([&]() { std::vector<Value> args; RunEventBody(*e, args); });
    }
}

bool Instance::BindParams(const EventDef& event, const EventParams& params, std::vector<Value>& outArgs) {
    outArgs.clear();
    for (size_t n = 0; n < event.params.size(); n++) {
        const ParamDef& declared = event.params[n];
        const std::string wanted = NormalizeParamName(declared.name);
        const std::string* text = nullptr;
        for (const auto& p : params) {
            if (NormalizeParamName(p.first) == wanted) { text = &p.second; break; }
        }
        if (!text && n < params.size()) text = &params[n].second;
        outArgs.push_back(text ? Deserialize(declared.type, *text) : Value::Default(declared.type));
    }
    return true;
}

bool Instance::RunCustomEvent(const std::string& name, const EventParams& params) {
    if (!program || !program->valid) return false;
    for (const auto& e : program->events) {
        if (e->kind != EventKind::Custom || e->name != name) continue;
        stepCount = 0;
        std::vector<Value> args;
        BindParams(*e, params, args);
        Guarded([&]() { RunEventBody(*e, args); });
        return true;
    }
    return false;
}

bool Instance::RunComponentEvent(Component* component, const std::string& eventName, const EventParams& params) {
    if (!program || !program->valid || !component) return false;
    std::string wantedName = NormalizeParamName(eventName);
    bool ran = false;
    for (const auto& e : program->events) {
        if (e->kind != EventKind::Component || e->componentName != component->GetName()) continue;
        if (NormalizeParamName(e->name) != wantedName) continue;
        stepCount = 0;
        std::vector<Value> args;
        BindParams(*e, params, args);
        Guarded([&]() { RunEventBody(*e, args); });
        ran = true;
    }
    return ran;
}

bool Instance::RunElementEvent(WidgetElement* target, const std::string& eventName) {
    if (!program || !program->valid || !target) return false;
    std::string wantedName = NormalizeParamName(eventName);
    bool ran = false;
    for (const auto& e : program->events) {
        if (e->kind != EventKind::Element || e->componentName != target->elementId) continue;
        if (NormalizeParamName(e->name) != wantedName) continue;
        stepCount = 0;
        std::vector<Value> args;
        Guarded([&]() { RunEventBody(*e, args); });
        ran = true;
    }
    return ran;
}

bool Instance::CallFunctionByName(const std::string& name, const EventParams& params, std::string& result) {
    if (!program || !program->valid) return false;
    auto it = program->funcIndex.find(name);
    if (it == program->funcIndex.end()) return false;
    const FuncDef& func = *program->funcs[(size_t)it->second];
    std::vector<Value> args;
    for (size_t n = 0; n < func.params.size(); n++) {
        const std::string wanted = NormalizeParamName(func.params[n].name);
        const std::string* text = nullptr;
        for (const auto& p : params) {
            if (NormalizeParamName(p.first) == wanted) { text = &p.second; break; }
        }
        if (!text && n < params.size()) text = &params[n].second;
        args.push_back(text ? Deserialize(func.params[n].type, *text) : Value::Default(func.params[n].type));
    }
    stepCount = 0;
    Value ret;
    bool ok = false;
    Guarded([&]() { ret = CallFunction(func, args); ok = true; });
    result = ok ? Serialize(ret) : std::string();
    return true;
}

static WidgetElement* SearchElementTree(WidgetElement* root, const std::string& id) {
    if (!root) return nullptr;
    if (root->elementId == id) return root;
    for (WidgetElement* child : root->GetAuthoredChildren()) {
        if (WidgetElement* found = SearchElementTree(child, id)) return found;
    }
    return nullptr;
}

WidgetElement* Instance::FindElementByName(const std::string& id) {
    if (!widget) return nullptr;
    auto it = elementCache.find(id);
    if (it != elementCache.end()) {
        WidgetElement* cached = dynamic_cast<WidgetElement*>(ObjectRefRegistry::Get().Resolve(it->second));
        if (cached && cached->owningWidget == widget && cached->elementId == id) return cached;
        elementCache.erase(it);
    }
    WidgetElement* found = SearchElementTree(widget->rootElement, id);
    if (found) elementCache[id] = found->GetObjectId();
    return found;
}

void Instance::CallEventFromScript(const EventDef& event, std::vector<Value>& args) {
    RunEventBody(event, args);

    NetworkBackend* backend = NetworkBackendRegistry::Get().GetActive();
    if (!backend || !owner || owner->GetNetId() == 0 || event.replication == ReplicationMode::Local) return;
    bool allowed = event.replication == ReplicationMode::Replicated || backend->HasAuthority();
    if (!allowed) return;

    ReplicatedEventCall call;
    call.netId = owner->GetNetId();
    call.eventName = event.name;
    call.reliable = event.reliable;
    for (size_t n = 0; n < event.params.size() && n < args.size(); n++)
        call.params.emplace_back(event.params[n].name, SerializeArg(args[n]));
    backend->SendEventCall(call, false);
}

Instance::Flow Instance::ExecBlock(const Block& block, Frame& frame) {
    for (const StmtPtr& s : block) {
        Flow flow = ExecStmt(*s, frame);
        if (flow != Flow::Normal) return flow;
    }
    return Flow::Normal;
}

Instance::Flow Instance::ExecStmt(const Stmt& s, Frame& frame) {
    Step();
    currentLine = s.line;
    switch (s.kind) {
        case StmtKind::VarDecl: {
            frame.locals[(size_t)s.slot] = s.init ? Coerce(s.varType, Eval(*s.init, frame)) : Value::Default(s.varType);
            return Flow::Normal;
        }
        case StmtKind::Assign: {
            Value rhs = Eval(*s.value, frame);
            if (s.op != Tok::Assign) {
                Tok op = s.op == Tok::PlusAssign ? Tok::Plus : s.op == Tok::MinusAssign ? Tok::Minus : s.op == Tok::StarAssign ? Tok::Star : Tok::Slash;
                Value current = Eval(*s.target, frame);
                rhs = BinaryOp(op, current, rhs);
            }
            AssignTo(*s.target, Coerce(s.target->type, rhs), frame);
            return Flow::Normal;
        }
        case StmtKind::ExprStmt: Eval(*s.value, frame); return Flow::Normal;
        case StmtKind::If: {
            for (size_t n = 0; n < s.conds.size(); n++) {
                if (Eval(*s.conds[n], frame).b) return ExecBlock(s.blocks[n], frame);
            }
            return ExecBlock(s.elseBlock, frame);
        }
        case StmtKind::While: {
            while (Eval(*s.conds[0], frame).b) {
                Step();
                Flow flow = ExecBlock(s.body, frame);
                if (flow == Flow::Break) break;
                if (flow == Flow::Return) return flow;
            }
            return Flow::Normal;
        }
        case StmtKind::For: {
            const Expr& it = *s.iterable;
            if (s.isRange) {
                long long start = 0, stop = 0, step = 1;
                std::vector<long long> a;
                for (const ExprPtr& arg : it.args) a.push_back(Eval(*arg, frame).i);
                if (a.size() == 1) stop = a[0];
                else if (a.size() >= 2) { start = a[0]; stop = a[1]; }
                if (a.size() == 3) step = a[2];
                if (step == 0) throw ScriptError{ "range() step cannot be 0" };
                for (long long i = start; step > 0 ? i < stop : i > stop; i += step) {
                    Step();
                    frame.locals[(size_t)s.slot] = Value::Int(i);
                    Flow flow = ExecBlock(s.body, frame);
                    if (flow == Flow::Break) break;
                    if (flow == Flow::Return) return flow;
                }
            } else {
                Value arr = Eval(it, frame);
                size_t count = arr.arr ? arr.arr->size() : 0;
                for (size_t i = 0; i < count && arr.arr && i < arr.arr->size(); i++) {
                    Step();
                    frame.locals[(size_t)s.slot] = (*arr.arr)[i];
                    Flow flow = ExecBlock(s.body, frame);
                    if (flow == Flow::Break) break;
                    if (flow == Flow::Return) return flow;
                }
            }
            return Flow::Normal;
        }
        case StmtKind::Return:
            if (s.value) frame.returnValue = Eval(*s.value, frame);
            return Flow::Return;
        case StmtKind::Break: return Flow::Break;
        case StmtKind::Continue: return Flow::Continue;
        case StmtKind::Pass: return Flow::Normal;
    }
    return Flow::Normal;
}

void Instance::AssignTo(const Expr& target, const Value& value, Frame& frame) {
    switch (target.kind) {
        case ExprKind::Ident:
            if (target.resolution == Resolution::Local) frame.locals[(size_t)target.slot] = value;
            else if (target.resolution == Resolution::Global) globals[(size_t)target.slot] = value;
            return;
        case ExprKind::Index: {
            Value base = Eval(*target.a, frame);
            long long idx = Eval(*target.b, frame).i;
            if (!base.arr || idx < 0 || idx >= (long long)base.arr->size()) throw ScriptError{ "Array index " + std::to_string(idx) + " is out of range" };
            (*base.arr)[(size_t)idx] = Coerce(MakeType(target.type.kind), value);
            return;
        }
        case ExprKind::Member: {
            if (target.resolution == Resolution::VecField) {
                Value base = Eval(*target.a, frame);
                float v = (float)value.AsNumber();
                if (target.slot == 0) base.x = v; else if (target.slot == 1) base.y = v; else base.z = v;
                AssignTo(*target.a, base, frame);
                return;
            }
            if (target.resolution == Resolution::ComponentField) {
                Value base = Eval(*target.a, frame);
                WriteComponentField(base, target.fieldKey, value);
                return;
            }
            if (target.resolution == Resolution::ElementField) {
                Value base = Eval(*target.a, frame);
                WriteElementField(base, target.fieldKey, value);
                return;
            }
            if (target.resolution == Resolution::Property && target.property && target.property->set) {
                Value base = Eval(*target.a, frame);
                Runtime rt{ owner, this };
                target.property->set(rt, base, value);
            }
            return;
        }
        default: return;
    }
}

Value Instance::BinaryOp(Tok op, const Value& l, const Value& r) {
    auto isNum = [](const Value& v) { return v.kind == Kind::Int || v.kind == Kind::Float; };

    if (op == Tok::Eq || op == Tok::Ne) {
        bool eq = false;
        if (isNum(l) && isNum(r)) eq = l.AsNumber() == r.AsNumber();
        else if (l.kind == Kind::Bool) eq = l.b == r.b;
        else if (l.kind == Kind::String) eq = l.s == r.s;
        else if (l.kind == Kind::Vec3) eq = l.x == r.x && l.y == r.y && l.z == r.z;
        else if (l.kind == Kind::Actor || l.kind == Kind::Component || l.kind == Kind::Element) eq = l.objectId == r.objectId;
        return Value::Bool(op == Tok::Eq ? eq : !eq);
    }

    if (op == Tok::Lt || op == Tok::Le || op == Tok::Gt || op == Tok::Ge) {
        int cmp;
        if (isNum(l)) { double a = l.AsNumber(), b = r.AsNumber(); cmp = a < b ? -1 : a > b ? 1 : 0; }
        else cmp = l.s.compare(r.s);
        switch (op) {
            case Tok::Lt: return Value::Bool(cmp < 0);
            case Tok::Le: return Value::Bool(cmp <= 0);
            case Tok::Gt: return Value::Bool(cmp > 0);
            default: return Value::Bool(cmp >= 0);
        }
    }

    if (l.kind == Kind::String && op == Tok::Plus) return Value::String(l.s + r.s);

    if (l.kind == Kind::Vec3 || r.kind == Kind::Vec3) {
        if (l.kind == Kind::Vec3 && r.kind == Kind::Vec3) {
            switch (op) {
                case Tok::Plus: return Value::Vec3(l.x + r.x, l.y + r.y, l.z + r.z);
                case Tok::Minus: return Value::Vec3(l.x - r.x, l.y - r.y, l.z - r.z);
                case Tok::Star: return Value::Vec3(l.x * r.x, l.y * r.y, l.z * r.z);
                default: break;
            }
        } else if (l.kind == Kind::Vec3) {
            float s = (float)r.AsNumber();
            if (op == Tok::Star) return Value::Vec3(l.x * s, l.y * s, l.z * s);
            if (op == Tok::Slash) return Value::Vec3(l.x / s, l.y / s, l.z / s);
        } else if (op == Tok::Star) {
            float s = (float)l.AsNumber();
            return Value::Vec3(r.x * s, r.y * s, r.z * s);
        }
        throw ScriptError{ "Unsupported Vec3 operation" };
    }

    if (l.kind == Kind::Int && r.kind == Kind::Int) {
        switch (op) {
            case Tok::Plus: return Value::Int(l.i + r.i);
            case Tok::Minus: return Value::Int(l.i - r.i);
            case Tok::Star: return Value::Int(l.i * r.i);
            case Tok::Slash:
                if (r.i == 0) throw ScriptError{ "Division by zero" };
                return Value::Int(l.i / r.i);
            case Tok::Percent:
                if (r.i == 0) throw ScriptError{ "Modulo by zero" };
                return Value::Int(l.i % r.i);
            default: break;
        }
    } else {
        double a = l.AsNumber(), b = r.AsNumber();
        switch (op) {
            case Tok::Plus: return Value::Float(a + b);
            case Tok::Minus: return Value::Float(a - b);
            case Tok::Star: return Value::Float(a * b);
            case Tok::Slash: return Value::Float(a / b);
            default: break;
        }
    }
    throw ScriptError{ "Unsupported operation" };
}

Value Instance::Eval(const Expr& e, Frame& frame) {
    Step();
    switch (e.kind) {
        case ExprKind::Literal: return e.literal;
        case ExprKind::SelfRef: {
            if (!owner) throw ScriptError{ "'self' is not available here" };
            return MakeActorValue(owner);
        }
        case ExprKind::Ident:
            if (e.resolution == Resolution::Local) return frame.locals[(size_t)e.slot];
            if (e.resolution == Resolution::Global) return globals[(size_t)e.slot];
            if (e.resolution == Resolution::Component) {
                Component* c = owner ? owner->FindComponentByName(e.name) : nullptr;
                if (!c) throw ScriptError{ "Component '" + e.name + "' does not exist on this actor" };
                return MakeComponentValue(c);
            }
            if (e.resolution == Resolution::Element) {
                WidgetElement* element = FindElementByName(e.name);
                if (!element) throw ScriptError{ "Element '" + e.name + "' does not exist in this widget" };
                return MakeElementValue(element);
            }
            throw ScriptError{ "Unresolved name '" + e.name + "'" };
        case ExprKind::Unary: {
            Value v = Eval(*e.a, frame);
            if (e.op == Tok::KwNot) return Value::Bool(!v.b);
            if (v.kind == Kind::Int) return Value::Int(-v.i);
            if (v.kind == Kind::Float) return Value::Float(-v.f);
            return Value::Vec3(-v.x, -v.y, -v.z);
        }
        case ExprKind::Binary: {
            if (e.op == Tok::KwAnd) {
                Value l = Eval(*e.a, frame);
                if (!l.b) return Value::Bool(false);
                return Value::Bool(Eval(*e.b, frame).b);
            }
            if (e.op == Tok::KwOr) {
                Value l = Eval(*e.a, frame);
                if (l.b) return Value::Bool(true);
                return Value::Bool(Eval(*e.b, frame).b);
            }
            Value l = Eval(*e.a, frame);
            Value r = Eval(*e.b, frame);
            return BinaryOp(e.op, l, r);
        }
        case ExprKind::Interpolation: {
            std::string out;
            for (const ExprPtr& piece : e.args) {
                if (piece->kind == ExprKind::Literal && piece->literal.kind == Kind::String) out += piece->literal.s;
                else out += ToDisplayString(Eval(*piece, frame));
            }
            return Value::String(out);
        }
        case ExprKind::ArrayLit: {
            Value arr = Value::Array();
            for (const ExprPtr& item : e.args) arr.arr->push_back(Coerce(MakeType(e.type.elem), Eval(*item, frame)));
            return arr;
        }
        case ExprKind::Index: {
            Value base = Eval(*e.a, frame);
            long long idx = Eval(*e.b, frame).i;
            if (e.resolution == Resolution::StringIndex) {
                if (idx < 0 || idx >= (long long)base.s.size()) throw ScriptError{ "String index " + std::to_string(idx) + " is out of range" };
                return Value::String(std::string(1, base.s[(size_t)idx]));
            }
            if (!base.arr || idx < 0 || idx >= (long long)base.arr->size()) throw ScriptError{ "Array index " + std::to_string(idx) + " is out of range" };
            return (*base.arr)[(size_t)idx];
        }
        case ExprKind::Member: {
            Value base = Eval(*e.a, frame);
            if (e.resolution == Resolution::VecField) return Value::Float(e.slot == 0 ? base.x : e.slot == 1 ? base.y : base.z);
            if (e.resolution == Resolution::ComponentField) return ReadComponentField(base, e.fieldKey);
            if (e.resolution == Resolution::ElementField) return ReadElementField(base, e.fieldKey);
            if (e.resolution == Resolution::Property && e.property) {
                Runtime rt{ owner, this };
                return e.property->get(rt, base);
            }
            throw ScriptError{ "Unresolved member '" + e.name + "'" };
        }
        case ExprKind::Call: return EvalCall(e, frame);
    }
    return Value::Void();
}

Value Instance::EvalCall(const Expr& e, Frame& frame) {
    std::vector<Value> args;
    args.reserve(e.args.size());

    switch (e.resolution) {
        case Resolution::Vec3Constructor: {
            for (const ExprPtr& a : e.args) args.push_back(Eval(*a, frame));
            return Value::Vec3((float)args[0].AsNumber(), (float)args[1].AsNumber(), (float)args[2].AsNumber());
        }
        case Resolution::UserFunction: {
            for (const ExprPtr& a : e.args) args.push_back(Eval(*a, frame));
            return CallFunction(*e.function, args);
        }
        case Resolution::EventCall: {
            for (const ExprPtr& a : e.args) args.push_back(Eval(*a, frame));
            CallEventFromScript(*e.eventDef, args);
            return Value::Void();
        }
        case Resolution::Builtin: {
            for (const ExprPtr& a : e.args) args.push_back(Eval(*a, frame));
            Runtime rt{ owner, this };
            return e.builtin->fn(rt, args);
        }
        case Resolution::Method: {
            Value self = Eval(*e.a->a, frame);
            for (const ExprPtr& a : e.args) args.push_back(Eval(*a, frame));
            Runtime rt{ owner, this };
            if (e.a->a->type.kind == Kind::Array) {
                Kind elem = e.a->a->type.elem;
                if (elem == Kind::Float) for (Value& v : args) if (v.kind == Kind::Int) v = Value::Float((double)v.i);
            }
            return e.method->fn(rt, self, args);
        }
        default: throw ScriptError{ "This call could not be resolved" };
    }
}

}
