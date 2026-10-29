#pragma once
#include "StarscriptAst.h"
#include "StarscriptBuiltins.h"
#include <memory>
#include <functional>
#include <set>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

class Actor;
class Component;
class Widget;
class WidgetElement;

namespace Starscript {

using EventParams = std::vector<std::pair<std::string, std::string>>;

class Instance {
public:
    explicit Instance(std::shared_ptr<Program> programToRun);

    void SetOwner(Actor* actor) { owner = actor; }
    Actor* GetOwner() const { return owner; }
    void SetWidget(Widget* newWidget) { widget = newWidget; elementCache.clear(); }
    Widget* GetWidget() const { return widget; }
    WidgetElement* FindElementByName(const std::string& id);
    const Program& GetProgram() const { return *program; }

    void RunBeginPlay();
    void RunTick(float deltaTime);
    void RunEndPlay();
    void RunDestroyed();
    bool RunCustomEvent(const std::string& name, const EventParams& params);
    bool RunComponentEvent(Component* component, const std::string& eventName, const EventParams& params);
    bool RunElementEvent(WidgetElement* target, const std::string& eventName);
    bool CallFunctionByName(const std::string& name, const EventParams& args, std::string& result);

    void CallEventFromScript(const EventDef& event, std::vector<Value>& args);
    bool FindCustomEvent(const std::string& name, ReplicationMode& outMode) const;

    int FindVariable(const std::string& name) const;
    Value& GlobalAt(int index) { return globals[(size_t)index]; }
    size_t GlobalCount() const { return globals.size(); }

    void ReportError(const std::string& message, int line);

    struct InputSample {
        bool held = false;
        bool prevHeld = false;
        float axis = 0.0f;
        float prevAxis = 0.0f;
        float x = 0.0f;
        float y = 0.0f;
        long long frame = -1;
    };
    const InputSample& QueryInput(const std::string& name);

    void SetTimer(const std::string& target, float interval, bool looping);
    void ClearTimer(const std::string& target);
    bool SetTimerPaused(const std::string& target, bool paused);
    bool IsTimerActive(const std::string& target) const;
    float TimerRemaining(const std::string& target) const;
    void CancelAsync();

private:
    struct Frame {
        std::vector<Value> locals;
        Value returnValue;
    };

    struct Level {
        enum class Type { Plain, While, ForRange, ForArray };
        Type type = Type::Plain;
        const Block* block = nullptr;
        size_t index = 0;
        const Stmt* stmt = nullptr;
        long long cur = 0, stop = 0, step = 1;
        Value array;
        size_t arrayIndex = 0;
    };

    struct Coroutine {
        Frame frame;
        std::vector<Level> stack;
        enum class Wait { None, Time, NextFrame } wait = Wait::None;
        float remaining = 0.0f;
    };

    struct Timer {
        std::string target;
        float interval = 0.0f;
        float remaining = 0.0f;
        bool looping = false;
        bool paused = false;
    };

    void StartCoroutine(const EventDef& event, std::vector<Value>& args);
    bool ResumeCoroutine(Coroutine& co);
    void UpdateCoroutines(float deltaTime);
    void UpdateTimers(float deltaTime);
    void FireTimerTarget(const std::string& target);
    void RefreshInput();
    void DispatchInputEvents();

    enum class Flow { Normal, Break, Continue, Return };

    void InitializeGlobals();
    void RunEventBody(const EventDef& event, std::vector<Value>& args);
    bool BindParams(const EventDef& event, const EventParams& params, std::vector<Value>& outArgs);
    Value CallFunction(const FuncDef& func, std::vector<Value>& args);

    Flow ExecBlock(const Block& block, Frame& frame);
    Flow ExecStmt(const Stmt& stmt, Frame& frame);
    Value Eval(const Expr& expr, Frame& frame);
    Value EvalCall(const Expr& expr, Frame& frame);
    Value BinaryOp(Tok op, const Value& l, const Value& r);
    void AssignTo(const Expr& target, const Value& value, Frame& frame);
    void Step();
    void Guarded(const std::function<void()>& fn);

    Value Coerce(const SType& target, Value value) const;

    std::shared_ptr<Program> program;
    Actor* owner = nullptr;
    Widget* widget = nullptr;
    std::unordered_map<std::string, unsigned> elementCache;
    std::vector<Value> globals;
    int stepCount = 0;
    int callDepth = 0;
    int currentLine = 0;
    bool initializing = false;
    std::set<std::pair<int, std::string>> reportedErrors;
    std::vector<std::unique_ptr<Coroutine>> coroutines;
    std::vector<Timer> timers;
    std::unordered_map<std::string, InputSample> inputSamples;
    long long inputFrame = 0;
};

}
