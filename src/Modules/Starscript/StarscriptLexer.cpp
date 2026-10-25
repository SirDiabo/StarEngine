#include "StarscriptLexer.h"
#include <cctype>
#include <unordered_map>

namespace Starscript {

const char* TokenName(Tok type) {
    switch (type) {
        case Tok::End: return "end of file";
        case Tok::Newline: return "end of line";
        case Tok::Indent: return "indent";
        case Tok::Dedent: return "dedent";
        case Tok::Ident: return "name";
        case Tok::Int: return "number";
        case Tok::Float: return "number";
        case Tok::String: return "string";
        case Tok::LParen: return "'('";
        case Tok::RParen: return "')'";
        case Tok::LBracket: return "'['";
        case Tok::RBracket: return "']'";
        case Tok::Comma: return "','";
        case Tok::Colon: return "':'";
        case Tok::Dot: return "'.'";
        case Tok::Arrow: return "'->'";
        case Tok::At: return "'@'";
        case Tok::Assign: return "'='";
        default: return "token";
    }
}

static const std::unordered_map<std::string, Tok>& Keywords() {
    static const std::unordered_map<std::string, Tok> table = {
        {"var", Tok::KwVar}, {"func", Tok::KwFunc}, {"event", Tok::KwEvent},
        {"if", Tok::KwIf}, {"elif", Tok::KwElif}, {"else", Tok::KwElse},
        {"while", Tok::KwWhile}, {"for", Tok::KwFor}, {"in", Tok::KwIn},
        {"return", Tok::KwReturn}, {"break", Tok::KwBreak}, {"continue", Tok::KwContinue},
        {"pass", Tok::KwPass}, {"and", Tok::KwAnd}, {"or", Tok::KwOr}, {"not", Tok::KwNot},
        {"true", Tok::KwTrue}, {"false", Tok::KwFalse}
    };
    return table;
}

std::vector<Token> Tokenize(const std::string& source, std::vector<Diagnostic>& diagnostics) {
    std::vector<Token> tokens;
    std::vector<int> indents = { 0 };
    int depth = 0;

    size_t pos = 0;
    int line = 1;
    bool atLineStart = true;

    auto push = [&](Tok type, const std::string& text, int ln, int col) {
        Token t;
        t.type = type;
        t.text = text;
        t.line = ln;
        t.column = col;
        tokens.push_back(t);
    };

    size_t lineStartPos = 0;

    while (pos < source.size()) {
        if (atLineStart && depth == 0) {
            int width = 0;
            size_t p = pos;
            while (p < source.size() && (source[p] == ' ' || source[p] == '\t')) {
                width += source[p] == '\t' ? 4 : 1;
                p++;
            }
            if (p >= source.size()) { pos = p; break; }
            if (source[p] == '\n' || source[p] == '\r' || source[p] == '#') {
                while (p < source.size() && source[p] != '\n') p++;
                if (p < source.size()) p++;
                pos = p;
                line++;
                lineStartPos = pos;
                continue;
            }
            pos = p;
            if (width > indents.back()) {
                indents.push_back(width);
                push(Tok::Indent, "", line, 1);
            } else {
                while (width < indents.back()) {
                    indents.pop_back();
                    push(Tok::Dedent, "", line, 1);
                }
                if (width != indents.back()) {
                    diagnostics.push_back({ line, 1, "Inconsistent indentation" });
                    indents.push_back(width);
                }
            }
            atLineStart = false;
        }

        char c = source[pos];
        int col = (int)(pos - lineStartPos) + 1;

        if (c == ' ' || c == '\t' || c == '\r') { pos++; continue; }

        if (c == '#') {
            while (pos < source.size() && source[pos] != '\n') pos++;
            continue;
        }

        if (c == '\n') {
            pos++;
            if (depth == 0) {
                if (!tokens.empty() && tokens.back().type != Tok::Newline && tokens.back().type != Tok::Indent && tokens.back().type != Tok::Dedent)
                    push(Tok::Newline, "", line, col);
                atLineStart = true;
            }
            line++;
            lineStartPos = pos;
            continue;
        }

        if (isdigit((unsigned char)c) || (c == '.' && pos + 1 < source.size() && isdigit((unsigned char)source[pos + 1]))) {
            size_t start = pos;
            bool isFloat = false;
            while (pos < source.size() && isdigit((unsigned char)source[pos])) pos++;
            if (pos < source.size() && source[pos] == '.' && pos + 1 < source.size() && isdigit((unsigned char)source[pos + 1])) {
                isFloat = true;
                pos++;
                while (pos < source.size() && isdigit((unsigned char)source[pos])) pos++;
            } else if (pos < source.size() && source[pos] == '.' && !(pos + 1 < source.size() && (isalpha((unsigned char)source[pos + 1]) || source[pos + 1] == '_'))) {
                isFloat = true;
                pos++;
            }
            if (pos < source.size() && (source[pos] == 'e' || source[pos] == 'E')) {
                size_t save = pos;
                pos++;
                if (pos < source.size() && (source[pos] == '+' || source[pos] == '-')) pos++;
                if (pos < source.size() && isdigit((unsigned char)source[pos])) {
                    isFloat = true;
                    while (pos < source.size() && isdigit((unsigned char)source[pos])) pos++;
                } else pos = save;
            }
            push(isFloat ? Tok::Float : Tok::Int, source.substr(start, pos - start), line, col);
            continue;
        }

        if (isalpha((unsigned char)c) || c == '_') {
            size_t start = pos;
            while (pos < source.size() && (isalnum((unsigned char)source[pos]) || source[pos] == '_')) pos++;
            std::string word = source.substr(start, pos - start);
            auto it = Keywords().find(word);
            push(it != Keywords().end() ? it->second : Tok::Ident, word, line, col);
            continue;
        }

        if (c == '"' || c == '\'') {
            char quote = c;
            pos++;
            std::string value;
            bool closed = false;
            while (pos < source.size() && source[pos] != '\n') {
                char ch = source[pos];
                if (ch == quote) { closed = true; pos++; break; }
                if (ch == '\\' && pos + 1 < source.size()) {
                    char n = source[pos + 1];
                    pos += 2;
                    switch (n) {
                        case 'n': value += '\n'; break;
                        case 't': value += '\t'; break;
                        case '\\': value += '\\'; break;
                        case '"': value += '"'; break;
                        case '\'': value += '\''; break;
                        default: value += n; break;
                    }
                    continue;
                }
                value += ch;
                pos++;
            }
            if (!closed) diagnostics.push_back({ line, col, "Unterminated string" });
            push(Tok::String, value, line, col);
            continue;
        }

        auto two = [&](char a, char b) { return c == a && pos + 1 < source.size() && source[pos + 1] == b; };
        auto emit = [&](Tok type, size_t len) {
            push(type, source.substr(pos, len), line, col);
            pos += len;
        };

        if (two('-', '>')) { emit(Tok::Arrow, 2); continue; }
        if (two(':', '=')) { emit(Tok::ColonAssign, 2); continue; }
        if (two('+', '=')) { emit(Tok::PlusAssign, 2); continue; }
        if (two('-', '=')) { emit(Tok::MinusAssign, 2); continue; }
        if (two('*', '=')) { emit(Tok::StarAssign, 2); continue; }
        if (two('/', '=')) { emit(Tok::SlashAssign, 2); continue; }
        if (two('=', '=')) { emit(Tok::Eq, 2); continue; }
        if (two('!', '=')) { emit(Tok::Ne, 2); continue; }
        if (two('<', '=')) { emit(Tok::Le, 2); continue; }
        if (two('>', '=')) { emit(Tok::Ge, 2); continue; }

        switch (c) {
            case '(': depth++; emit(Tok::LParen, 1); continue;
            case ')': if (depth > 0) depth--; emit(Tok::RParen, 1); continue;
            case '[': depth++; emit(Tok::LBracket, 1); continue;
            case ']': if (depth > 0) depth--; emit(Tok::RBracket, 1); continue;
            case ',': emit(Tok::Comma, 1); continue;
            case ':': emit(Tok::Colon, 1); continue;
            case '.': emit(Tok::Dot, 1); continue;
            case '@': emit(Tok::At, 1); continue;
            case '=': emit(Tok::Assign, 1); continue;
            case '+': emit(Tok::Plus, 1); continue;
            case '-': emit(Tok::Minus, 1); continue;
            case '*': emit(Tok::Star, 1); continue;
            case '/': emit(Tok::Slash, 1); continue;
            case '%': emit(Tok::Percent, 1); continue;
            case '<': emit(Tok::Lt, 1); continue;
            case '>': emit(Tok::Gt, 1); continue;
            default: break;
        }

        diagnostics.push_back({ line, col, std::string("Unexpected character '") + c + "'" });
        pos++;
    }

    int endCol = (int)(pos - lineStartPos) + 1;
    if (!tokens.empty() && tokens.back().type != Tok::Newline && tokens.back().type != Tok::Dedent)
        push(Tok::Newline, "", line, endCol);
    while (indents.size() > 1) {
        indents.pop_back();
        push(Tok::Dedent, "", line, endCol);
    }
    push(Tok::End, "", line, endCol);
    return tokens;
}

}
