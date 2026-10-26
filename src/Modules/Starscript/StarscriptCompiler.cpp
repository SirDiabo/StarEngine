#include "StarscriptCompiler.h"
#include "StarscriptParser.h"
#include "StarscriptBuiltins.h"
#include "Runtime/ComponentEvents.h"
#include "Runtime/ComponentRegistry.h"
#include "Core/ObjectKindRegistry.h"
#include <algorithm>
#include <unordered_set>

namespace Starscript {

std::string NormalizeEventName(const std::string& name) {
    std::string out;
    for (char c : name) {
        if (c == ' ' || c == '_') continue;
        out += (char)tolower((unsigned char)c);
    }
    return out;
}

namespace {

SType EventTypeToScript(const ComponentEventParam& param) {
    switch (param.type) {
        case EventValueType::Bool: return MakeType(Kind::Bool);
        case EventValueType::Int: return MakeType(Kind::Int);
        case EventValueType::Float: return MakeType(Kind::Float);
        case EventValueType::Vec3:
        case EventValueType::Rotator: return MakeType(Kind::Vec3);
        case EventValueType::Object:
            if (param.objectKind.empty() || param.objectKind == "Actor") return MakeObjectType(Kind::Actor, "Actor");
            return MakeObjectType(Kind::Component, param.objectKind);
        default: return MakeType(Kind::String);
    }
}

struct LocalVar {
    int slot;
    SType type;
};

class Compiler {
public:
    Compiler(Program& prog, const std::vector<ComponentDecl>& comps) : program(prog), components(comps) {
        for (const ComponentDecl& c : comps) componentTypes[c.name] = c.componentType;
    }

    void Run() {
        ResolveDeclarations();
        for (auto& f : program.funcs) CheckFunction(*f);
        for (auto& e : program.events) CheckEvent(*e);
        program.valid = program.diagnostics.empty();
    }

private:
    Program& program;
    const std::vector<ComponentDecl>& components;
    std::unordered_map<std::string, std::string> componentTypes;
    std::unordered_map<std::string, const EventDef*> customEvents;

    std::vector<std::unordered_map<std::string, LocalVar>> scopes;
    int nextSlot = 0;
    int maxSlot = 0;
    const FuncDef* currentFunc = nullptr;
    SType currentRet;
    int loopDepth = 0;
    bool inInitializer = false;

    void Error(int line, int column, const std::string& msg) {
        program.diagnostics.push_back({ line, column, msg });
    }

    bool ResolveType(const TypeRef& ref, SType& out, bool allowVoid = false) {
        if (!ref.present) { if (allowVoid) { out = MakeType(Kind::Void); return true; } return false; }
        if (ref.isArray) {
            SType elem;
            if (!ParseTypeName(ref.elemName, elem) || elem.kind == Kind::Array || elem.kind == Kind::Actor) {
                Error(ref.line, ref.column, "Unknown array element type '" + ref.elemName + "'");
                return false;
            }
            out = MakeArrayType(elem.kind);
            return true;
        }
        if (ParseTypeName(ref.name, out)) return true;
        if (ref.name == "Component" || ref.name == "SceneComponent" || ComponentRegistry::Get().Find(ref.name)) {
            out = MakeObjectType(Kind::Component, ref.name);
            return true;
        }
        Error(ref.line, ref.column, "Unknown type '" + ref.name + "'");
        return false;
    }

    void ResolveDeclarations() {
        std::unordered_set<std::string> taken;
        for (auto& kv : componentTypes) taken.insert(kv.first);

        for (auto& v : program.vars) {
            if (taken.count(v->name) || program.varIndex.count(v->name)) {
                Error(v->line, v->column, "'" + v->name + "' is already declared");
                continue;
            }
            program.varIndex[v->name] = (int)(&v - &program.vars[0]);
            if (v->typeRef.present) ResolveType(v->typeRef, v->type);
            ProcessVarAnnotations(*v);
        }

        inInitializer = true;
        for (auto& v : program.vars) {
            BeginBody();
            if (v->init) {
                SType initType = CheckExpr(*v->init, v->typeRef.present ? &v->type : nullptr);
                if (v->typeRef.present) {
                    if (initType.kind != Kind::Void && v->type.kind != Kind::Void && !IsAssignable(v->type, initType))
                        Error(v->line, v->column, "Cannot assign " + initType.ToString() + " to " + v->type.ToString());
                } else {
                    if (initType.kind == Kind::Void) Error(v->line, v->column, "Cannot infer a type for '" + v->name + "'");
                    else if (initType.kind == Kind::Array && initType.elem == Kind::Void) Error(v->line, v->column, "Cannot infer the element type of an empty array; declare a type");
                    v->type = initType;
                }
            } else if (!v->typeRef.present) {
                Error(v->line, v->column, "Variable '" + v->name + "' needs a type or an initial value");
            }
            if (v->replication != ReplicationMode::Local && (v->type.kind == Kind::Array || v->type.kind == Kind::Actor || v->type.kind == Kind::Component))
                Error(v->line, v->column, "Variables of type " + v->type.ToString() + " cannot be replicated");
            if (!v->saveFile.empty() && (v->type.kind == Kind::Actor || v->type.kind == Kind::Component))
                Error(v->line, v->column, "Variables of type " + v->type.ToString() + " cannot be saved");
        }
        inInitializer = false;

        for (auto& f : program.funcs) {
            if (taken.count(f->name) || program.varIndex.count(f->name) || program.funcIndex.count(f->name) || FindBuiltin(f->name)) {
                Error(f->line, f->column, "'" + f->name + "' is already declared");
            } else {
                program.funcIndex[f->name] = (int)(&f - &program.funcs[0]);
            }
            for (ParamDef& p : f->params) ResolveType(p.typeRef, p.type);
            if (f->retRef.present) ResolveType(f->retRef, f->ret);
            else f->ret = MakeType(Kind::Void);
        }

        std::unordered_set<std::string> seenEvents;
        for (auto& e : program.events) {
            for (ParamDef& p : e->params) ResolveType(p.typeRef, p.type);
            ProcessEventAnnotations(*e);
            ValidateEvent(*e);

            std::string key = e->componentName + "." + NormalizeEventName(e->name);
            if (e->kind == EventKind::Custom) {
                if (taken.count(e->name) || program.varIndex.count(e->name) || program.funcIndex.count(e->name) || FindBuiltin(e->name)) {
                    Error(e->line, e->column, "'" + e->name + "' is already declared");
                }
            }
            if (!seenEvents.insert(key).second) Error(e->line, e->column, "Event '" + e->name + "' is declared twice");
            if (e->kind == EventKind::Custom) customEvents[e->name] = e.get();
        }
    }

    void ProcessVarAnnotations(VarDef& v) {
        for (const Annotation& a : v.annotations) {
            if (a.name == "replicated") v.replication = ReplicationMode::Replicated;
            else if (a.name == "server_only") v.replication = ReplicationMode::ServerOnly;
            else if (a.name == "save") {
                if (!a.hasArg || a.arg.empty()) Error(a.line, a.column, "@save needs a save file name, e.g. @save(\"player\")");
                else v.saveFile = a.arg;
            } else {
                Error(a.line, a.column, "Unknown variable annotation '@" + a.name + "'");
            }
        }
    }

    void ProcessEventAnnotations(EventDef& e) {
        for (const Annotation& a : e.annotations) {
            if (e.kind != EventKind::Custom) {
                Error(a.line, a.column, "Annotations are only supported on custom events");
                continue;
            }
            if (a.name == "replicated") e.replication = ReplicationMode::Replicated;
            else if (a.name == "server_only") e.replication = ReplicationMode::ServerOnly;
            else if (a.name == "reliable") e.reliable = true;
            else Error(a.line, a.column, "Unknown event annotation '@" + a.name + "'");
        }
    }

    void ValidateEvent(const EventDef& e) {
        switch (e.kind) {
            case EventKind::BeginPlay:
            case EventKind::EndPlay:
            case EventKind::Destroyed:
                if (!e.params.empty()) Error(e.line, e.column, "Event '" + e.name + "' takes no parameters");
                break;
            case EventKind::Tick:
                if (e.params.size() > 1 || (e.params.size() == 1 && e.params[0].type.kind != Kind::Float))
                    Error(e.line, e.column, "Event 'Tick' takes either no parameters or one float (delta time)");
                break;
            case EventKind::Component: {
                auto it = componentTypes.find(e.componentName);
                if (it == componentTypes.end()) {
                    Error(e.line, e.column, "Unknown component '" + e.componentName + "'");
                    break;
                }
                const ComponentEventDecl* decl = nullptr;
                for (const ComponentEventDecl& d : ComponentEventRegistry::Get().GetAll()) {
                    if (d.componentType == it->second && NormalizeEventName(d.name) == NormalizeEventName(e.name)) { decl = &d; break; }
                }
                if (!decl) {
                    Error(e.line, e.column, "Component '" + e.componentName + "' (" + it->second + ") has no event '" + e.name + "'");
                    break;
                }
                if (decl->params.size() != e.params.size()) {
                    Error(e.line, e.column, "Event '" + e.name + "' takes " + std::to_string(decl->params.size()) + " parameter(s)");
                    break;
                }
                for (size_t n = 0; n < decl->params.size(); n++) {
                    if (!IsAssignable(e.params[n].type, EventTypeToScript(decl->params[n])))
                        Error(e.line, e.column, "Parameter '" + e.params[n].name + "' should be " + EventTypeToScript(decl->params[n]).ToString());
                }
                break;
            }
            default: break;
        }
    }

    void PushScope() { scopes.emplace_back(); }
    void PopScope() { scopes.pop_back(); }

    int Declare(const std::string& name, const SType& type, int line, int column) {
        if (scopes.back().count(name)) Error(line, column, "'" + name + "' is already declared in this scope");
        int slot = nextSlot++;
        if (nextSlot > maxSlot) maxSlot = nextSlot;
        scopes.back()[name] = { slot, type };
        return slot;
    }

    const LocalVar* FindLocal(const std::string& name) const {
        for (size_t n = scopes.size(); n-- > 0;) {
            auto it = scopes[n].find(name);
            if (it != scopes[n].end()) return &it->second;
        }
        return nullptr;
    }

    void BeginBody() {
        scopes.clear();
        PushScope();
        nextSlot = 0;
        maxSlot = 0;
        loopDepth = 0;
    }

    void CheckFunction(FuncDef& f) {
        BeginBody();
        currentFunc = &f;
        currentRet = f.ret;
        for (ParamDef& p : f.params) Declare(p.name, p.type, f.line, f.column);
        CheckBlock(f.body);
        f.numLocals = maxSlot;
        currentFunc = nullptr;
    }

    void CheckEvent(EventDef& e) {
        BeginBody();
        currentFunc = nullptr;
        currentRet = MakeType(Kind::Void);
        for (ParamDef& p : e.params) Declare(p.name, p.type, e.line, e.column);
        CheckBlock(e.body);
        e.numLocals = maxSlot;
    }

    void CheckBlock(Block& block) {
        PushScope();
        int saved = nextSlot;
        for (StmtPtr& s : block) CheckStmt(*s);
        nextSlot = saved;
        PopScope();
    }

    bool IsBool(const SType& t) { return t.kind == Kind::Bool; }

    void CheckCondition(Expr* expr) {
        if (!expr) return;
        Expr& e = *expr;
        SType t = CheckExpr(e, nullptr);
        if (t.kind != Kind::Bool && t.kind != Kind::Void) Error(e.line, e.column, "Condition must be bool, got " + t.ToString());
    }

    void CheckStmt(Stmt& s) {
        switch (s.kind) {
            case StmtKind::VarDecl: {
                SType declared;
                bool hasDeclared = s.declType.present && ResolveType(s.declType, declared);
                SType type;
                if (s.init) {
                    SType initType = CheckExpr(*s.init, hasDeclared ? &declared : nullptr);
                    if (hasDeclared) {
                        if (initType.kind != Kind::Void && !IsAssignable(declared, initType))
                            Error(s.line, s.column, "Cannot assign " + initType.ToString() + " to " + declared.ToString());
                        type = declared;
                    } else {
                        if (initType.kind == Kind::Void) Error(s.line, s.column, "Cannot infer a type for '" + s.name + "'");
                        type = initType;
                        if (type.kind == Kind::Array && type.elem == Kind::Void) Error(s.line, s.column, "Cannot infer the element type of an empty array; declare a type");
                    }
                } else if (hasDeclared) {
                    type = declared;
                } else {
                    Error(s.line, s.column, "Variable '" + s.name + "' needs a type or an initial value");
                }
                s.varType = type;
                s.slot = Declare(s.name, type, s.line, s.column);
                break;
            }
            case StmtKind::Assign: CheckAssign(s); break;
            case StmtKind::ExprStmt: {
                if (!s.value) break;
                if (s.value->kind != ExprKind::Call) {
                    Error(s.line, s.column, "This expression has no effect");
                    CheckExpr(*s.value, nullptr);
                } else {
                    CheckExpr(*s.value, nullptr);
                }
                break;
            }
            case StmtKind::If:
                for (size_t n = 0; n < s.conds.size() && n < s.blocks.size(); n++) {
                    CheckCondition(s.conds[n].get());
                    CheckBlock(s.blocks[n]);
                }
                CheckBlock(s.elseBlock);
                break;
            case StmtKind::While:
                if (!s.conds.empty()) CheckCondition(s.conds[0].get());
                loopDepth++;
                CheckBlock(s.body);
                loopDepth--;
                break;
            case StmtKind::For: CheckFor(s); break;
            case StmtKind::Return: {
                if (s.value) {
                    SType t = CheckExpr(*s.value, &currentRet);
                    if (currentRet.kind == Kind::Void) Error(s.line, s.column, "This function does not return a value");
                    else if (t.kind != Kind::Void && !IsAssignable(currentRet, t))
                        Error(s.line, s.column, "Cannot return " + t.ToString() + " from a function returning " + currentRet.ToString());
                } else if (currentRet.kind != Kind::Void) {
                    Error(s.line, s.column, "Expected a return value of type " + currentRet.ToString());
                }
                break;
            }
            case StmtKind::Break:
            case StmtKind::Continue:
                if (loopDepth == 0) Error(s.line, s.column, "'break' and 'continue' can only be used inside a loop");
                break;
            case StmtKind::Pass: break;
        }
    }

    void CheckFor(Stmt& s) {
        SType loopType = MakeType(Kind::Int);
        if (!s.iterable) return;
        Expr& it = *s.iterable;
        if (it.kind == ExprKind::Call && it.a && it.a->kind == ExprKind::Ident && it.a->name == "range") {
            s.isRange = true;
            if (it.args.empty() || it.args.size() > 3) Error(it.line, it.column, "range() takes 1 to 3 integer arguments");
            for (ExprPtr& arg : it.args) {
                SType t = CheckExpr(*arg, nullptr);
                if (t.kind != Kind::Int) Error(arg->line, arg->column, "range() arguments must be int");
            }
            it.type = MakeType(Kind::Void);
        } else {
            SType t = CheckExpr(it, nullptr);
            if (t.kind != Kind::Array) {
                if (t.kind != Kind::Void) Error(it.line, it.column, "Can only loop over range() or an array, got " + t.ToString());
            } else {
                loopType = MakeType(t.elem);
            }
        }
        PushScope();
        int saved = nextSlot;
        s.slot = Declare(s.name, loopType, s.line, s.column);
        loopDepth++;
        CheckBlock(s.body);
        loopDepth--;
        nextSlot = saved;
        PopScope();
    }

    bool IsLValue(Expr& e) {
        switch (e.kind) {
            case ExprKind::Ident: return e.resolution == Resolution::Local || e.resolution == Resolution::Global;
            case ExprKind::Index: return e.resolution == Resolution::ArrayIndex;
            case ExprKind::Member:
                if (e.resolution == Resolution::VecField) return IsLValue(*e.a) || e.a->resolution == Resolution::Property;
                if (e.resolution == Resolution::Property) return e.property && e.property->set;
                return false;
            default: return false;
        }
    }

    void CheckAssign(Stmt& s) {
        SType targetType = CheckExpr(*s.target, nullptr);
        bool lvalue = IsLValue(*s.target);
        if (s.target->resolution == Resolution::VecField && s.target->a->kind == ExprKind::Member &&
            s.target->a->resolution == Resolution::Property && !(s.target->a->property && s.target->a->property->set)) lvalue = false;
        if (!lvalue && targetType.kind != Kind::Void) Error(s.target->line, s.target->column, "Cannot assign to this expression");
        SType valueType = CheckExpr(*s.value, &targetType);
        if (targetType.kind == Kind::Void || valueType.kind == Kind::Void) return;

        if (s.op == Tok::Assign) {
            if (!IsAssignable(targetType, valueType))
                Error(s.line, s.column, "Cannot assign " + valueType.ToString() + " to " + targetType.ToString());
            return;
        }
        Tok binary = s.op == Tok::PlusAssign ? Tok::Plus : s.op == Tok::MinusAssign ? Tok::Minus : s.op == Tok::StarAssign ? Tok::Star : Tok::Slash;
        SType result = BinaryType(binary, targetType, valueType, s.line, s.column);
        if (result.kind != Kind::Void && !IsAssignable(targetType, result))
            Error(s.line, s.column, "Cannot store " + result.ToString() + " in " + targetType.ToString());
    }

    SType BinaryType(Tok op, const SType& l, const SType& r, int line, int col) {
        auto bad = [&]() {
            Error(line, col, "Operator is not defined for " + l.ToString() + " and " + r.ToString());
            return MakeType(Kind::Void);
        };
        switch (op) {
            case Tok::Plus:
                if (l.IsNumeric() && r.IsNumeric()) return (l.kind == Kind::Int && r.kind == Kind::Int) ? MakeType(Kind::Int) : MakeType(Kind::Float);
                if (l.kind == Kind::String && r.kind == Kind::String) return MakeType(Kind::String);
                if (l.kind == Kind::Vec3 && r.kind == Kind::Vec3) return MakeType(Kind::Vec3);
                return bad();
            case Tok::Minus:
                if (l.IsNumeric() && r.IsNumeric()) return (l.kind == Kind::Int && r.kind == Kind::Int) ? MakeType(Kind::Int) : MakeType(Kind::Float);
                if (l.kind == Kind::Vec3 && r.kind == Kind::Vec3) return MakeType(Kind::Vec3);
                return bad();
            case Tok::Star:
                if (l.IsNumeric() && r.IsNumeric()) return (l.kind == Kind::Int && r.kind == Kind::Int) ? MakeType(Kind::Int) : MakeType(Kind::Float);
                if (l.kind == Kind::Vec3 && r.IsNumeric()) return MakeType(Kind::Vec3);
                if (l.IsNumeric() && r.kind == Kind::Vec3) return MakeType(Kind::Vec3);
                if (l.kind == Kind::Vec3 && r.kind == Kind::Vec3) return MakeType(Kind::Vec3);
                return bad();
            case Tok::Slash:
                if (l.IsNumeric() && r.IsNumeric()) return (l.kind == Kind::Int && r.kind == Kind::Int) ? MakeType(Kind::Int) : MakeType(Kind::Float);
                if (l.kind == Kind::Vec3 && r.IsNumeric()) return MakeType(Kind::Vec3);
                return bad();
            case Tok::Percent:
                if (l.kind == Kind::Int && r.kind == Kind::Int) return MakeType(Kind::Int);
                return bad();
            case Tok::Lt: case Tok::Le: case Tok::Gt: case Tok::Ge:
                if (l.IsNumeric() && r.IsNumeric()) return MakeType(Kind::Bool);
                if (l.kind == Kind::String && r.kind == Kind::String) return MakeType(Kind::Bool);
                return bad();
            case Tok::Eq: case Tok::Ne:
                if (l.IsNumeric() && r.IsNumeric()) return MakeType(Kind::Bool);
                if (l.kind == r.kind && l.kind != Kind::Array) return MakeType(Kind::Bool);
                return bad();
            case Tok::KwAnd: case Tok::KwOr:
                if (l.kind == Kind::Bool && r.kind == Kind::Bool) return MakeType(Kind::Bool);
                return bad();
            default: return bad();
        }
    }

    SType CheckExpr(Expr& e, const SType* expected) {
        e.type = CheckExprInner(e, expected);
        return e.type;
    }

    SType CheckExprInner(Expr& e, const SType* expected) {
        switch (e.kind) {
            case ExprKind::Literal: return MakeType(e.literal.kind);
            case ExprKind::SelfRef:
                if (inInitializer) Error(e.line, e.column, "'self' is not available in a variable initializer");
                return MakeObjectType(Kind::Actor, "Actor");
            case ExprKind::Ident: return CheckIdent(e);
            case ExprKind::Unary: {
                SType t = CheckExpr(*e.a, nullptr);
                if (t.kind == Kind::Void) return t;
                if (e.op == Tok::KwNot) {
                    if (t.kind != Kind::Bool) { Error(e.line, e.column, "'not' needs a bool"); return MakeType(Kind::Void); }
                    return t;
                }
                if (!t.IsNumeric() && t.kind != Kind::Vec3) { Error(e.line, e.column, "Cannot negate " + t.ToString()); return MakeType(Kind::Void); }
                return t;
            }
            case ExprKind::Binary: {
                SType l = CheckExpr(*e.a, nullptr);
                SType r = CheckExpr(*e.b, nullptr);
                if (l.kind == Kind::Void || r.kind == Kind::Void) return MakeType(Kind::Void);
                return BinaryType(e.op, l, r, e.line, e.column);
            }
            case ExprKind::ArrayLit: return CheckArrayLiteral(e, expected);
            case ExprKind::Index: return CheckIndex(e);
            case ExprKind::Member: return CheckMember(e);
            case ExprKind::Call: return CheckCall(e);
        }
        return MakeType(Kind::Void);
    }

    SType CheckIdent(Expr& e) {
        if (const LocalVar* local = FindLocal(e.name)) {
            e.resolution = Resolution::Local;
            e.slot = local->slot;
            return local->type;
        }
        auto vit = program.varIndex.find(e.name);
        if (vit != program.varIndex.end()) {
            e.resolution = Resolution::Global;
            e.slot = vit->second;
            return program.vars[vit->second]->type;
        }
        auto cit = componentTypes.find(e.name);
        if (cit != componentTypes.end()) {
            if (inInitializer) Error(e.line, e.column, "Components are not available in a variable initializer");
            e.resolution = Resolution::Component;
            return MakeObjectType(Kind::Component, cit->second);
        }
        if (program.funcIndex.count(e.name) || customEvents.count(e.name))
            Error(e.line, e.column, "'" + e.name + "' is a function; call it with ()");
        else
            Error(e.line, e.column, "Unknown name '" + e.name + "'");
        return MakeType(Kind::Void);
    }

    SType CheckArrayLiteral(Expr& e, const SType* expected) {
        Kind elem = Kind::Void;
        if (expected && expected->kind == Kind::Array) elem = expected->elem;
        for (ExprPtr& item : e.args) {
            SType t = CheckExpr(*item, nullptr);
            if (t.kind == Kind::Void) continue;
            if (elem == Kind::Void) elem = t.kind;
            else if (!IsAssignable(MakeType(elem), t)) Error(item->line, item->column, "Array element should be " + MakeType(elem).ToString() + ", got " + t.ToString());
        }
        if (elem == Kind::Actor || elem == Kind::Component || elem == Kind::Array) {
            Error(e.line, e.column, "Arrays of this element type are not supported");
            elem = Kind::Void;
        }
        return MakeArrayType(elem);
    }

    SType CheckIndex(Expr& e) {
        SType base = CheckExpr(*e.a, nullptr);
        SType idx = CheckExpr(*e.b, nullptr);
        if (idx.kind != Kind::Int && idx.kind != Kind::Void) Error(e.b->line, e.b->column, "Index must be int");
        if (base.kind == Kind::Array) { e.resolution = Resolution::ArrayIndex; return MakeType(base.elem); }
        if (base.kind == Kind::String) { e.resolution = Resolution::StringIndex; return MakeType(Kind::String); }
        if (base.kind != Kind::Void) Error(e.line, e.column, "Cannot index " + base.ToString());
        return MakeType(Kind::Void);
    }

    SType CheckMember(Expr& e) {
        SType base = CheckExpr(*e.a, nullptr);
        if (base.kind == Kind::Void) return base;
        if (base.kind == Kind::Vec3 && (e.name == "x" || e.name == "y" || e.name == "z")) {
            e.resolution = Resolution::VecField;
            e.slot = e.name[0] - 'x';
            return MakeType(Kind::Float);
        }
        if (const PropertyDef* prop = FindProperty(base.kind, e.name)) {
            e.resolution = Resolution::Property;
            e.property = prop;
            return prop->type;
        }
        Error(e.line, e.column, base.ToString() + " has no property '" + e.name + "'");
        return MakeType(Kind::Void);
    }

    SType CheckCall(Expr& e) {
        Expr& callee = *e.a;

        if (callee.kind == ExprKind::Ident) {
            const std::string& name = callee.name;
            if (name == "Vec3") {
                e.resolution = Resolution::Vec3Constructor;
                if (e.args.size() != 3) Error(e.line, e.column, "Vec3() takes 3 numbers");
                for (ExprPtr& a : e.args) {
                    SType t = CheckExpr(*a, nullptr);
                    if (t.kind != Kind::Void && !t.IsNumeric()) Error(a->line, a->column, "Vec3() arguments must be numbers");
                }
                return MakeType(Kind::Vec3);
            }

            auto fit = program.funcIndex.find(name);
            if ((fit != program.funcIndex.end() || customEvents.count(name)) && inInitializer) {
                Error(e.line, e.column, "Cannot call '" + name + "' in a variable initializer");
                for (ExprPtr& a : e.args) CheckExpr(*a, nullptr);
                return MakeType(Kind::Void);
            }
            if (fit != program.funcIndex.end() && !FindLocal(name)) {
                const FuncDef& f = *program.funcs[fit->second];
                e.resolution = Resolution::UserFunction;
                e.function = &f;
                CheckArgs(e, f.params, name);
                return f.ret;
            }

            auto eit = customEvents.find(name);
            if (eit != customEvents.end()) {
                e.resolution = Resolution::EventCall;
                e.eventDef = eit->second;
                CheckArgs(e, eit->second->params, name);
                return MakeType(Kind::Void);
            }

            if (const BuiltinDef* b = FindBuiltin(name)) {
                e.resolution = Resolution::Builtin;
                e.builtin = b;
                return CheckBuiltin(e, *b);
            }

            Error(callee.line, callee.column, "Unknown function '" + name + "'");
            for (ExprPtr& a : e.args) CheckExpr(*a, nullptr);
            return MakeType(Kind::Void);
        }

        if (callee.kind == ExprKind::Member) {
            SType recv = CheckExpr(*callee.a, nullptr);
            if (recv.kind == Kind::Void) {
                for (ExprPtr& a : e.args) CheckExpr(*a, nullptr);
                return recv;
            }
            const MethodDef* m = FindMethod(recv.kind, callee.name);
            if (!m) {
                Error(callee.line, callee.column, recv.ToString() + " has no method '" + callee.name + "'");
                for (ExprPtr& a : e.args) CheckExpr(*a, nullptr);
                return MakeType(Kind::Void);
            }
            e.resolution = Resolution::Method;
            e.method = m;
            if (e.args.size() < m->minArgs || e.args.size() > m->params.size())
                Error(e.line, e.column, std::string("'") + m->name + "' takes " + std::to_string(m->minArgs) +
                      (m->minArgs == m->params.size() ? "" : "-" + std::to_string(m->params.size())) + " argument(s)");
            for (size_t n = 0; n < e.args.size(); n++) {
                SType want = n < m->params.size() ? m->params[n] : MakeType(Kind::Any);
                if (m->elemParam && n == 0) want = MakeType(recv.elem);
                SType got = CheckExpr(*e.args[n], &want);
                if (got.kind != Kind::Void && !IsAssignable(want, got))
                    Error(e.args[n]->line, e.args[n]->column, "Argument " + std::to_string(n + 1) + " should be " + want.ToString() + ", got " + got.ToString());
            }
            if (m->rule == RetRule::ElemType) return MakeType(recv.elem);
            return m->ret;
        }

        Error(e.line, e.column, "This expression cannot be called");
        return MakeType(Kind::Void);
    }

    void CheckArgs(Expr& e, const std::vector<ParamDef>& params, const std::string& name) {
        if (e.args.size() != params.size())
            Error(e.line, e.column, "'" + name + "' takes " + std::to_string(params.size()) + " argument(s), got " + std::to_string(e.args.size()));
        for (size_t n = 0; n < e.args.size(); n++) {
            SType want = n < params.size() ? params[n].type : MakeType(Kind::Any);
            SType got = CheckExpr(*e.args[n], &want);
            if (n < params.size() && got.kind != Kind::Void && want.kind != Kind::Void && !IsAssignable(want, got))
                Error(e.args[n]->line, e.args[n]->column, "Argument " + std::to_string(n + 1) + " should be " + want.ToString() + ", got " + got.ToString());
        }
    }

    SType CheckBuiltin(Expr& e, const BuiltinDef& b) {
        size_t maxArgs = b.params.size();
        if (e.args.size() < b.minArgs || (!b.variadic && e.args.size() > maxArgs))
            Error(e.line, e.column, std::string("'") + b.name + "' takes " + std::to_string(b.minArgs) +
                  ((b.minArgs == maxArgs || b.variadic) ? (b.variadic ? "+" : "") : "-" + std::to_string(maxArgs)) + " argument(s)");
        bool allInt = true;
        for (size_t n = 0; n < e.args.size(); n++) {
            SType want = n < maxArgs ? b.params[n] : MakeType(Kind::Any);
            SType got = CheckExpr(*e.args[n], &want);
            if (got.kind == Kind::Void) { allInt = false; continue; }
            if (got.kind != Kind::Int) allInt = false;
            if (b.rule == RetRule::Numeric) {
                if (!got.IsNumeric()) Error(e.args[n]->line, e.args[n]->column, std::string("'") + b.name + "' needs numbers");
            } else if (!IsAssignable(want, got)) {
                Error(e.args[n]->line, e.args[n]->column, "Argument " + std::to_string(n + 1) + " should be " + want.ToString() + ", got " + got.ToString());
            }
        }
        if (b.rule == RetRule::Numeric) return MakeType(allInt ? Kind::Int : Kind::Float);
        return b.ret;
    }
};

}

std::shared_ptr<Program> Compile(const std::string& source, const std::vector<ComponentDecl>& components) {
    std::unique_ptr<Program> program = Parse(source);
    Compiler compiler(*program, components);
    compiler.Run();
    std::stable_sort(program->diagnostics.begin(), program->diagnostics.end(),
        [](const Diagnostic& a, const Diagnostic& b) { return a.line < b.line || (a.line == b.line && a.column < b.column); });
    program->valid = program->diagnostics.empty();
    return std::shared_ptr<Program>(std::move(program));
}

}
