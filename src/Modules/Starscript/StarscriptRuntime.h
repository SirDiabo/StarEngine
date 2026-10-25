#pragma once
#include "StarscriptAst.h"
#include "StarscriptBuiltins.h"
#include <memory>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

class Actor;
class Component;

namespace Starscript {

using EventParams = std::vector<std::pair<std::string, std::string>>;

class Instance {
public:
    explicit Instance(std::shared_ptr<Program> programToRun);

    void SetOwner(Actor* actor) { owner = actor; }
    Actor* GetOwner() const { return owner; }
    const Program& GetProgram() const { return *program; }

    void RunBeginPlay();
    void RunTick(float deltaTime);
    void RunEndPlay();
    void RunDestroyed();
    bool RunCustomEvent(const std::string& name, const EventParams& params);
    bool RunComponentEvent(Component* component, const std::string& eventName, const EventParams& params);

    void CallEventFromScript(const EventDef& event, std::vector<Value>& args);
    bool FindCustomEvent(const std::string& name, ReplicationMode& outMode) const;

    int FindVariable(const std::string& name) const;
    Value& GlobalAt(int index) { return globals[(size_t)index]; }
    size_t GlobalCount() const { return globals.size(); }

    void ReportError(const std::string& message, int line);

private:
    struct Frame {
        std::vector<Value> locals;
        Value returnValue;
    };

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
    std::vector<Value> globals;
    int stepCount = 0;
    int callDepth = 0;
    int currentLine = 0;
    bool initializing = false;
    std::set<std::pair<int, std::string>> reportedErrors;
};

}
