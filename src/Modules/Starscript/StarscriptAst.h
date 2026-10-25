#pragma once
#include "StarscriptTypes.h"
#include "StarscriptLexer.h"
#include "Runtime/ReplicationMode.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Starscript {

struct BuiltinDef;
struct MethodDef;
struct PropertyDef;
struct FuncDef;
struct EventDef;

struct TypeRef {
    std::string name;
    std::string elemName;
    bool isArray = false;
    bool present = false;
    int line = 0;
    int column = 0;
};

enum class ExprKind { Literal, Ident, SelfRef, Unary, Binary, Call, Member, Index, ArrayLit };

enum class Resolution {
    None, Local, Global, Component, UserFunction, EventCall, Builtin, Vec3Constructor,
    Method, Property, VecField, ArrayIndex, StringIndex
};

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Expr {
    ExprKind kind = ExprKind::Literal;
    int line = 0;
    int column = 0;
    SType type;

    Value literal;
    std::string name;
    Tok op = Tok::End;
    ExprPtr a;
    ExprPtr b;
    std::vector<ExprPtr> args;

    Resolution resolution = Resolution::None;
    int slot = -1;
    const BuiltinDef* builtin = nullptr;
    const MethodDef* method = nullptr;
    const PropertyDef* property = nullptr;
    const FuncDef* function = nullptr;
    const EventDef* eventDef = nullptr;
};

enum class StmtKind { VarDecl, Assign, ExprStmt, If, While, For, Return, Break, Continue, Pass };

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;
using Block = std::vector<StmtPtr>;

struct Stmt {
    StmtKind kind = StmtKind::Pass;
    int line = 0;
    int column = 0;

    std::string name;
    TypeRef declType;
    SType varType;
    int slot = -1;
    ExprPtr init;

    ExprPtr target;
    Tok op = Tok::Assign;
    ExprPtr value;

    std::vector<ExprPtr> conds;
    std::vector<Block> blocks;
    Block elseBlock;

    ExprPtr iterable;
    bool isRange = false;
    Block body;
};

struct Annotation {
    std::string name;
    std::string arg;
    bool hasArg = false;
    int line = 0;
    int column = 0;
};

struct ParamDef {
    std::string name;
    TypeRef typeRef;
    SType type;
};

struct VarDef {
    std::string name;
    TypeRef typeRef;
    SType type;
    ExprPtr init;
    std::vector<Annotation> annotations;
    ReplicationMode replication = ReplicationMode::Local;
    std::string saveFile;
    int line = 0;
    int column = 0;
};

struct FuncDef {
    std::string name;
    std::vector<ParamDef> params;
    TypeRef retRef;
    SType ret;
    Block body;
    int numLocals = 0;
    int line = 0;
    int column = 0;
};

enum class EventKind { BeginPlay, Tick, EndPlay, Destroyed, Component, Custom };

struct EventDef {
    EventKind kind = EventKind::Custom;
    std::string name;
    std::string componentName;
    std::vector<ParamDef> params;
    Block body;
    int numLocals = 0;
    std::vector<Annotation> annotations;
    ReplicationMode replication = ReplicationMode::Local;
    bool reliable = false;
    int line = 0;
    int column = 0;
};

struct Program {
    std::vector<std::unique_ptr<VarDef>> vars;
    std::vector<std::unique_ptr<FuncDef>> funcs;
    std::vector<std::unique_ptr<EventDef>> events;
    std::unordered_map<std::string, int> varIndex;
    std::unordered_map<std::string, int> funcIndex;
    std::vector<Diagnostic> diagnostics;
    bool valid = false;
};

}
