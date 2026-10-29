#include "StarscriptParser.h"
#include <cstdlib>

namespace Starscript {

namespace {

class Parser {
public:
    Parser(std::vector<Token> toks, Program& prog) : tokens(std::move(toks)), program(prog) {}

    void ParseProgram() {
        while (!At(Tok::End)) {
            if (At(Tok::Newline)) { Advance(); continue; }
            if (At(Tok::Indent) || At(Tok::Dedent)) {
                if (At(Tok::Indent)) Error("Unexpected indentation");
                Advance();
                continue;
            }
            size_t before = pos;
            ParseTopLevel();
            if (pos == before) Advance();
        }
    }

private:
    std::vector<Token> tokens;
    size_t pos = 0;
    Program& program;
    std::vector<Annotation> pendingAnnotations;

    const Token& Cur() const { return tokens[pos]; }
    bool At(Tok t) const { return tokens[pos].type == t; }
    const Token& Advance() { const Token& t = tokens[pos]; if (pos + 1 < tokens.size()) pos++; return t; }

    bool Match(Tok t) {
        if (!At(t)) return false;
        Advance();
        return true;
    }

    void Error(const std::string& message) {
        program.diagnostics.push_back({ Cur().line, Cur().column, message });
    }

    bool Expect(Tok t, const std::string& what) {
        if (At(t)) { Advance(); return true; }
        Error("Expected " + what);
        return false;
    }

    void SkipLine() {
        int depth = 0;
        while (!At(Tok::End)) {
            if (At(Tok::Indent)) depth++;
            else if (At(Tok::Dedent)) { if (depth == 0) return; depth--; }
            else if (At(Tok::Newline) && depth == 0) { Advance(); return; }
            Advance();
        }
    }

    void SkipLineKeepBlock() {
        while (!At(Tok::End) && !At(Tok::Newline)) Advance();
        if (At(Tok::Newline)) Advance();
        if (At(Tok::Indent)) {
            int depth = 0;
            while (!At(Tok::End)) {
                if (At(Tok::Indent)) depth++;
                else if (At(Tok::Dedent)) { depth--; if (depth == 0) { Advance(); return; } }
                Advance();
            }
        }
    }

    void ParseTopLevel() {
        if (At(Tok::At)) {
            ParseAnnotation();
            return;
        }
        if (At(Tok::KwVar)) { ParseGlobalVar(); return; }
        if (At(Tok::KwFunc)) { ParseFunc(); return; }
        if (At(Tok::KwEvent)) { ParseEvent(); return; }
        Error("Expected 'var', 'func' or 'event' at the top level");
        SkipLineKeepBlock();
        pendingAnnotations.clear();
    }

    void ParseAnnotation() {
        Annotation ann;
        ann.line = Cur().line;
        ann.column = Cur().column;
        Advance();
        if (!At(Tok::Ident)) { Error("Expected annotation name after '@'"); SkipLine(); return; }
        ann.name = Advance().text;
        if (Match(Tok::LParen)) {
            if (At(Tok::String) || At(Tok::Ident)) {
                ann.arg = Advance().text;
                ann.hasArg = true;
            }
            Expect(Tok::RParen, "')'");
        }
        pendingAnnotations.push_back(ann);
        if (!At(Tok::Newline) && !At(Tok::End)) Error("Unexpected text after annotation");
        while (!At(Tok::Newline) && !At(Tok::End)) Advance();
        Match(Tok::Newline);
    }

    TypeRef ParseTypeRef() {
        TypeRef ref;
        ref.line = Cur().line;
        ref.column = Cur().column;
        if (!At(Tok::Ident)) { Error("Expected a type name"); return ref; }
        ref.name = Advance().text;
        ref.present = true;
        if (ref.name == "Array" && Match(Tok::LBracket)) {
            ref.isArray = true;
            if (At(Tok::Ident)) ref.elemName = Advance().text;
            else Error("Expected an element type");
            Expect(Tok::RBracket, "']'");
        }
        return ref;
    }

    void ParseParams(std::vector<ParamDef>& out) {
        Expect(Tok::LParen, "'('");
        while (!At(Tok::RParen) && !At(Tok::End) && !At(Tok::Newline)) {
            ParamDef param;
            if (!At(Tok::Ident)) { Error("Expected a parameter name"); break; }
            param.name = Advance().text;
            if (Expect(Tok::Colon, "':' and a type for the parameter")) param.typeRef = ParseTypeRef();
            out.push_back(std::move(param));
            if (!Match(Tok::Comma)) break;
        }
        Expect(Tok::RParen, "')'");
    }

    void ParseGlobalVar() {
        auto def = std::make_unique<VarDef>();
        def->line = Cur().line;
        def->column = Cur().column;
        def->annotations = std::move(pendingAnnotations);
        pendingAnnotations.clear();
        Advance();
        if (!At(Tok::Ident)) { Error("Expected a variable name"); SkipLine(); return; }
        def->name = Advance().text;
        if (Match(Tok::Colon)) def->typeRef = ParseTypeRef();
        if (Match(Tok::ColonAssign)) {
            def->init = ParseExpr();
        } else if (Match(Tok::Assign)) {
            def->init = ParseExpr();
        }
        if (!At(Tok::Newline) && !At(Tok::End)) Error("Unexpected text after variable declaration");
        while (!At(Tok::Newline) && !At(Tok::End)) Advance();
        Match(Tok::Newline);
        program.vars.push_back(std::move(def));
    }

    bool ParseBlock(Block& out) {
        if (!Expect(Tok::Colon, "':'")) { SkipLineKeepBlock(); return false; }
        if (!At(Tok::Newline)) {
            StmtPtr stmt = ParseStatement();
            if (stmt) out.push_back(std::move(stmt));
            return true;
        }
        Advance();
        if (!At(Tok::Indent)) { Error("Expected an indented block"); return false; }
        Advance();
        while (!At(Tok::Dedent) && !At(Tok::End)) {
            if (At(Tok::Newline)) { Advance(); continue; }
            size_t before = pos;
            StmtPtr stmt = ParseStatement();
            if (stmt) out.push_back(std::move(stmt));
            if (pos == before) Advance();
        }
        Match(Tok::Dedent);
        return true;
    }

    void ParseFunc() {
        auto def = std::make_unique<FuncDef>();
        def->line = Cur().line;
        def->column = Cur().column;
        pendingAnnotations.clear();
        Advance();
        if (!At(Tok::Ident)) { Error("Expected a function name"); SkipLineKeepBlock(); return; }
        def->name = Advance().text;
        ParseParams(def->params);
        if (Match(Tok::Arrow)) def->retRef = ParseTypeRef();
        ParseBlock(def->body);
        program.funcs.push_back(std::move(def));
    }

    void ParseEvent() {
        auto def = std::make_unique<EventDef>();
        def->line = Cur().line;
        def->column = Cur().column;
        def->annotations = std::move(pendingAnnotations);
        pendingAnnotations.clear();
        Advance();
        std::string first;
        if (At(Tok::String) && pos + 1 < tokens.size() && tokens[pos + 1].type == Tok::Dot) {
            first = Advance().text;
        } else {
            if (!At(Tok::Ident)) { Error("Expected an event name"); SkipLineKeepBlock(); return; }
            first = Advance().text;
        }
        if (first == "Input" && At(Tok::LParen) && pos + 1 < tokens.size() && tokens[pos + 1].type == Tok::String) {
            Advance();
            def->kind = EventKind::Input;
            def->name = "Input";
            def->inputName = Advance().text;
            if (Match(Tok::Comma)) {
                while (!At(Tok::RParen) && !At(Tok::End) && !At(Tok::Newline)) {
                    ParamDef param;
                    if (!At(Tok::Ident)) { Error("Expected a parameter name"); break; }
                    param.name = Advance().text;
                    if (Expect(Tok::Colon, "':' and a type for the parameter")) param.typeRef = ParseTypeRef();
                    def->params.push_back(std::move(param));
                    if (!Match(Tok::Comma)) break;
                }
            }
            Expect(Tok::RParen, "')'");
            ParseBlock(def->body);
            program.events.push_back(std::move(def));
            return;
        }
        if (Match(Tok::Dot)) {
            def->componentName = first;
            if (!At(Tok::Ident)) { Error("Expected an event name after '.'"); SkipLineKeepBlock(); return; }
            def->name = Advance().text;
            def->kind = EventKind::Component;
        } else {
            def->name = first;
            if (first == "BeginPlay" || first == "Construct") def->kind = EventKind::BeginPlay;
            else if (first == "Tick") def->kind = EventKind::Tick;
            else if (first == "EndPlay" || first == "Destruct") def->kind = EventKind::EndPlay;
            else if (first == "Destroyed") def->kind = EventKind::Destroyed;
            else def->kind = EventKind::Custom;
        }
        ParseParams(def->params);
        ParseBlock(def->body);
        program.events.push_back(std::move(def));
    }

    StmtPtr MakeStmt(StmtKind kind) {
        auto s = std::make_unique<Stmt>();
        s->kind = kind;
        s->line = Cur().line;
        s->column = Cur().column;
        return s;
    }

    void EndOfStatement() {
        if (At(Tok::Newline)) { Advance(); return; }
        if (At(Tok::End) || At(Tok::Dedent)) return;
        Error("Unexpected text at end of statement");
        while (!At(Tok::Newline) && !At(Tok::End) && !At(Tok::Dedent)) Advance();
        Match(Tok::Newline);
    }

    StmtPtr ParseStatement() {
        switch (Cur().type) {
            case Tok::KwVar: return ParseLocalVar();
            case Tok::KwIf: return ParseIf();
            case Tok::KwWhile: {
                auto s = MakeStmt(StmtKind::While);
                Advance();
                ExprPtr cond = ParseExpr();
                if (!cond) { SkipLineKeepBlock(); return nullptr; }
                s->conds.push_back(std::move(cond));
                ParseBlock(s->body);
                return s;
            }
            case Tok::KwFor: return ParseFor();
            case Tok::KwReturn: {
                auto s = MakeStmt(StmtKind::Return);
                Advance();
                if (!At(Tok::Newline) && !At(Tok::End) && !At(Tok::Dedent)) s->value = ParseExpr();
                EndOfStatement();
                return s;
            }
            case Tok::KwAwait: {
                auto s = MakeStmt(StmtKind::Await);
                Advance();
                s->value = ParseExpr();
                if (!s->value) { SkipLine(); return nullptr; }
                EndOfStatement();
                return s;
            }
            case Tok::KwBreak: { auto s = MakeStmt(StmtKind::Break); Advance(); EndOfStatement(); return s; }
            case Tok::KwContinue: { auto s = MakeStmt(StmtKind::Continue); Advance(); EndOfStatement(); return s; }
            case Tok::KwPass: { auto s = MakeStmt(StmtKind::Pass); Advance(); EndOfStatement(); return s; }
            case Tok::KwFunc:
            case Tok::KwEvent:
                Error("'func' and 'event' can only be declared at the top level");
                SkipLineKeepBlock();
                return nullptr;
            default: break;
        }

        auto s = MakeStmt(StmtKind::ExprStmt);
        ExprPtr expr = ParseExpr();
        if (!expr) { SkipLine(); return nullptr; }
        Tok t = Cur().type;
        if (t == Tok::Assign || t == Tok::PlusAssign || t == Tok::MinusAssign || t == Tok::StarAssign || t == Tok::SlashAssign) {
            s->kind = StmtKind::Assign;
            s->op = t;
            Advance();
            s->target = std::move(expr);
            s->value = ParseExpr();
            if (!s->value) { SkipLine(); return nullptr; }
        } else {
            s->value = std::move(expr);
        }
        EndOfStatement();
        return s;
    }

    StmtPtr ParseLocalVar() {
        auto s = MakeStmt(StmtKind::VarDecl);
        Advance();
        if (!At(Tok::Ident)) { Error("Expected a variable name"); SkipLine(); return nullptr; }
        s->name = Advance().text;
        if (Match(Tok::Colon)) s->declType = ParseTypeRef();
        if (Match(Tok::ColonAssign) || Match(Tok::Assign)) s->init = ParseExpr();
        EndOfStatement();
        return s;
    }

    StmtPtr ParseIf() {
        auto s = MakeStmt(StmtKind::If);
        Advance();
        ExprPtr first = ParseExpr();
        if (!first) { SkipLineKeepBlock(); return nullptr; }
        s->conds.push_back(std::move(first));
        s->blocks.emplace_back();
        ParseBlock(s->blocks.back());
        while (At(Tok::KwElif)) {
            Advance();
            ExprPtr cond = ParseExpr();
            if (!cond) { SkipLineKeepBlock(); continue; }
            s->conds.push_back(std::move(cond));
            s->blocks.emplace_back();
            ParseBlock(s->blocks.back());
        }
        if (At(Tok::KwElse)) {
            Advance();
            ParseBlock(s->elseBlock);
        }
        return s;
    }

    StmtPtr ParseFor() {
        auto s = MakeStmt(StmtKind::For);
        Advance();
        if (!At(Tok::Ident)) { Error("Expected a loop variable name"); SkipLineKeepBlock(); return nullptr; }
        s->name = Advance().text;
        if (!Expect(Tok::KwIn, "'in'")) { SkipLineKeepBlock(); return nullptr; }
        s->iterable = ParseExpr();
        if (!s->iterable) { SkipLineKeepBlock(); return nullptr; }
        ParseBlock(s->body);
        return s;
    }

    ExprPtr MakeExpr(ExprKind kind, const Token& at) {
        auto e = std::make_unique<Expr>();
        e->kind = kind;
        e->line = at.line;
        e->column = at.column;
        return e;
    }

    ExprPtr ParseExpr() { return ParseOr(); }

    ExprPtr MakeBinary(Tok op, const Token& at, ExprPtr lhs, ExprPtr rhs) {
        auto e = MakeExpr(ExprKind::Binary, at);
        e->op = op;
        e->a = std::move(lhs);
        e->b = std::move(rhs);
        return e;
    }

    ExprPtr ParseOr() {
        ExprPtr lhs = ParseAnd();
        while (lhs && At(Tok::KwOr)) {
            Token op = Advance();
            ExprPtr rhs = ParseAnd();
            if (!rhs) return nullptr;
            lhs = MakeBinary(Tok::KwOr, op, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    ExprPtr ParseAnd() {
        ExprPtr lhs = ParseNot();
        while (lhs && At(Tok::KwAnd)) {
            Token op = Advance();
            ExprPtr rhs = ParseNot();
            if (!rhs) return nullptr;
            lhs = MakeBinary(Tok::KwAnd, op, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    ExprPtr ParseNot() {
        if (At(Tok::KwNot)) {
            Token op = Advance();
            auto e = MakeExpr(ExprKind::Unary, op);
            e->op = Tok::KwNot;
            e->a = ParseNot();
            if (!e->a) return nullptr;
            return e;
        }
        return ParseComparison();
    }

    ExprPtr ParseComparison() {
        ExprPtr lhs = ParseAdditive();
        while (lhs && (At(Tok::Eq) || At(Tok::Ne) || At(Tok::Lt) || At(Tok::Le) || At(Tok::Gt) || At(Tok::Ge))) {
            Token op = Advance();
            ExprPtr rhs = ParseAdditive();
            if (!rhs) return nullptr;
            lhs = MakeBinary(op.type, op, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    ExprPtr ParseAdditive() {
        ExprPtr lhs = ParseMultiplicative();
        while (lhs && (At(Tok::Plus) || At(Tok::Minus))) {
            Token op = Advance();
            ExprPtr rhs = ParseMultiplicative();
            if (!rhs) return nullptr;
            lhs = MakeBinary(op.type, op, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    ExprPtr ParseMultiplicative() {
        ExprPtr lhs = ParseUnary();
        while (lhs && (At(Tok::Star) || At(Tok::Slash) || At(Tok::Percent))) {
            Token op = Advance();
            ExprPtr rhs = ParseUnary();
            if (!rhs) return nullptr;
            lhs = MakeBinary(op.type, op, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    ExprPtr ParseUnary() {
        if (At(Tok::Minus)) {
            Token op = Advance();
            auto e = MakeExpr(ExprKind::Unary, op);
            e->op = Tok::Minus;
            e->a = ParseUnary();
            if (!e->a) return nullptr;
            return e;
        }
        return ParsePostfix();
    }

    ExprPtr ParsePostfix() {
        ExprPtr expr = ParsePrimary();
        while (expr) {
            if (At(Tok::LParen)) {
                Token at = Advance();
                auto call = MakeExpr(ExprKind::Call, at);
                call->line = expr->line;
                call->column = expr->column;
                call->a = std::move(expr);
                while (!At(Tok::RParen) && !At(Tok::End) && !At(Tok::Newline)) {
                    ExprPtr arg = ParseExpr();
                    if (!arg) return nullptr;
                    call->args.push_back(std::move(arg));
                    if (!Match(Tok::Comma)) break;
                }
                if (!Expect(Tok::RParen, "')'")) return nullptr;
                expr = std::move(call);
            } else if (At(Tok::Dot)) {
                Token at = Advance();
                if (!At(Tok::Ident)) { Error("Expected a name after '.'"); return nullptr; }
                auto member = MakeExpr(ExprKind::Member, at);
                member->name = Advance().text;
                member->line = expr->line;
                member->column = expr->column;
                member->a = std::move(expr);
                expr = std::move(member);
            } else if (At(Tok::LBracket)) {
                Token at = Advance();
                auto index = MakeExpr(ExprKind::Index, at);
                index->line = expr->line;
                index->column = expr->column;
                index->a = std::move(expr);
                index->b = ParseExpr();
                if (!index->b) return nullptr;
                if (!Expect(Tok::RBracket, "']'")) return nullptr;
                expr = std::move(index);
            } else {
                break;
            }
        }
        return expr;
    }

    ExprPtr ParseInterpolation(const Token& tok) {
        auto node = MakeExpr(ExprKind::Interpolation, tok);
        std::vector<StringPart> parts = tok.parts;
        Advance();
        for (const StringPart& part : parts) {
            if (!part.isExpression) {
                auto lit = MakeExpr(ExprKind::Literal, tok);
                lit->literal = Value::String(part.text);
                node->args.push_back(std::move(lit));
                continue;
            }
            ExprPtr inner = ParseEmbedded(part);
            if (!inner) return nullptr;
            node->args.push_back(std::move(inner));
        }
        return node;
    }

    ExprPtr ParseEmbedded(const StringPart& part) {
        size_t lead = part.text.find_first_not_of(" \t");
        std::string text = part.text.substr(lead == std::string::npos ? 0 : lead);
        int columnOffset = part.column + (lead == std::string::npos ? 0 : (int)lead) - 1;

        std::vector<Diagnostic> lexDiagnostics;
        std::vector<Token> sub = Tokenize(text, lexDiagnostics);
        for (Diagnostic& d : lexDiagnostics) program.diagnostics.push_back({ part.line, d.column + columnOffset, d.message });
        for (Token& t : sub) { t.line = part.line; t.column += columnOffset; }

        Parser nested(std::move(sub), program);
        ExprPtr expr = nested.ParseExpr();
        if (!expr) return nullptr;
        while (nested.At(Tok::Newline)) nested.Advance();
        if (!nested.At(Tok::End)) {
            program.diagnostics.push_back({ part.line, nested.Cur().column, "Unexpected text inside '{ }' in string" });
            return nullptr;
        }
        return expr;
    }

    ExprPtr ParsePrimary() {
        const Token& tok = Cur();
        switch (tok.type) {
            case Tok::Int: {
                auto e = MakeExpr(ExprKind::Literal, tok);
                e->literal = Value::Int(strtoll(tok.text.c_str(), nullptr, 10));
                Advance();
                return e;
            }
            case Tok::Float: {
                auto e = MakeExpr(ExprKind::Literal, tok);
                e->literal = Value::Float(strtod(tok.text.c_str(), nullptr));
                Advance();
                return e;
            }
            case Tok::String: {
                if (!tok.parts.empty()) return ParseInterpolation(tok);
                auto e = MakeExpr(ExprKind::Literal, tok);
                e->literal = Value::String(tok.text);
                Advance();
                return e;
            }
            case Tok::KwTrue:
            case Tok::KwFalse: {
                auto e = MakeExpr(ExprKind::Literal, tok);
                e->literal = Value::Bool(tok.type == Tok::KwTrue);
                Advance();
                return e;
            }
            case Tok::Ident: {
                if (tok.text == "self") {
                    auto e = MakeExpr(ExprKind::SelfRef, tok);
                    Advance();
                    return e;
                }
                auto e = MakeExpr(ExprKind::Ident, tok);
                e->name = tok.text;
                Advance();
                return e;
            }
            case Tok::LParen: {
                Advance();
                ExprPtr inner = ParseExpr();
                if (!inner) return nullptr;
                if (!Expect(Tok::RParen, "')'")) return nullptr;
                return inner;
            }
            case Tok::LBracket: {
                auto e = MakeExpr(ExprKind::ArrayLit, tok);
                Advance();
                while (!At(Tok::RBracket) && !At(Tok::End)) {
                    ExprPtr item = ParseExpr();
                    if (!item) return nullptr;
                    e->args.push_back(std::move(item));
                    if (!Match(Tok::Comma)) break;
                }
                if (!Expect(Tok::RBracket, "']'")) return nullptr;
                return e;
            }
            default:
                Error(std::string("Unexpected ") + TokenName(tok.type) + " in expression");
                return nullptr;
        }
    }
};

}

std::unique_ptr<Program> Parse(const std::string& source) {
    auto program = std::make_unique<Program>();
    std::vector<Token> tokens = Tokenize(source, program->diagnostics);
    Parser parser(std::move(tokens), *program);
    parser.ParseProgram();
    return program;
}

}
