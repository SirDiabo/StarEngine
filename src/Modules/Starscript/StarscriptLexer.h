#pragma once
#include <string>
#include <vector>

namespace Starscript {

enum class Tok {
    End, Newline, Indent, Dedent,
    Ident, Int, Float, String,
    KwVar, KwFunc, KwEvent, KwIf, KwElif, KwElse, KwWhile, KwFor, KwIn, KwReturn,
    KwBreak, KwContinue, KwPass, KwAnd, KwOr, KwNot, KwTrue, KwFalse, KwAwait,
    LParen, RParen, LBracket, RBracket, Comma, Colon, Dot, Arrow, At,
    Assign, ColonAssign, PlusAssign, MinusAssign, StarAssign, SlashAssign,
    Plus, Minus, Star, Slash, Percent,
    Eq, Ne, Lt, Le, Gt, Ge,
    Error
};

struct StringPart {
    bool isExpression = false;
    std::string text;
    int line = 1;
    int column = 1;
};

struct Token {
    Tok type = Tok::End;
    std::string text;
    int line = 1;
    int column = 1;
    std::vector<StringPart> parts;
};

struct Diagnostic {
    int line = 0;
    int column = 0;
    std::string message;
};

std::vector<Token> Tokenize(const std::string& source, std::vector<Diagnostic>& diagnostics);
const char* TokenName(Tok type);

}
