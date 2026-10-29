#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

struct CodeDiagnostic {
    int line = 0;
    int column = 0;
    std::string message;
};

enum class CompletionKind { Keyword, Type, Function, Variable, Event, Component, Property, Method, Snippet };

struct CompletionItem {
    std::string label;
    std::string insertText;
    std::string detail;
    CompletionKind kind = CompletionKind::Keyword;
};

struct CompletionRequest {
    bool afterDot = false;
    bool inEventHeader = false;
    std::string receiver;
};

enum class TokenClass { Plain, Keyword, Type, Builtin, Function, Variable, Component, Number, String, Comment, Annotation, Event, Self };

class CodeEditor {
public:
    CodeEditor();

    void SetText(const std::string& text);
    const std::string& GetText();
    int GetVersion() const { return version; }
    int LineCount() const { return (int)lines.size(); }
    const std::string& LineText(int index) const { return lines[(size_t)index].text; }

    std::string GetViewState() const;
    void SetViewState(const std::string& state);

    void GotoLine(int line, int column = 0);
    void AppendText(const std::string& text);
    void SelectRange(int line, int column, int length);
    int CaretLine() const { return caret.line; }
    int CaretColumn() const { return caret.col; }
    int SelectionLength() const;
    bool InsertTextAtPoint(Vector2 point, const std::string& text);

    void SetDiagnostics(std::vector<CodeDiagnostic> newDiagnostics);
    const std::vector<CodeDiagnostic>& GetDiagnostics() const { return diagnostics; }

    void Render(Rectangle area);
    bool IsTyping() const { return typingTimer > 0.0f; }

    std::function<void()> onTextChanged;
    std::function<void()> onSaveRequested;
    std::function<void(const CompletionRequest&, std::vector<CompletionItem>&)> completionProvider;
    std::function<TokenClass(const std::string& name, bool afterDot, bool callFollows)> classifyIdentifier;

private:
    struct Pos {
        int line = 0;
        int col = 0;
        bool operator==(const Pos& o) const { return line == o.line && col == o.col; }
        bool operator!=(const Pos& o) const { return !(*this == o); }
        bool operator<(const Pos& o) const { return line < o.line || (line == o.line && col < o.col); }
    };

    struct Span {
        int start;
        int end;
        TokenClass cls;
    };

    struct Line {
        std::string text;
        std::vector<float> offsets;
        float width = 0.0f;
        bool metricsValid = false;
        std::vector<Span> spans;
        bool spansValid = false;
    };

    void EnsureFontMetrics();
    float CharAdvance(unsigned char c);
    void EnsureMetrics(Line& line);
    void EnsureSpans(Line& line);
    float XForCol(int lineIndex, int col);
    int ColForX(int lineIndex, float x);
    Pos PosAtPoint(Vector2 point);

    Pos ClampPos(Pos p) const;
    Pos SelStart() const { return anchor < caret ? anchor : caret; }
    Pos SelEnd() const { return anchor < caret ? caret : anchor; }
    bool HasSelection() const { return anchor != caret; }
    std::string TextInRange(Pos a, Pos b) const;
    void ReplaceRange(Pos a, Pos b, const std::string& text);
    void InsertText(const std::string& text);
    void InsertChar(unsigned codepoint);
    void DeleteBackward(bool wholeWord);
    void DeleteForward(bool wholeWord);
    void NewLineWithIndent();
    void IndentSelection(bool outdent);
    void ToggleComment();
    void DuplicateLines();
    void MoveLines(int direction);
    void DeleteLines();
    void SelectAll();
    void SelectWordAt(Pos p);
    void MarkChanged();

    int PrevBoundary(const std::string& s, int index) const;
    int NextBoundary(const std::string& s, int index) const;
    int WordLeft(Pos p) const;
    int WordRight(Pos p) const;
    int LeadingSpaces(const std::string& s) const;

    void MoveCaret(Pos target, bool extend, bool keepDesiredX = false);
    void MoveVertical(int deltaLines, bool extend);
    void HandleKeyboard();
    void HandleMouse(Rectangle textArea, Rectangle vBar, Rectangle hBar);
    void RevealCaret(Rectangle textArea);
    void ClampScroll(Rectangle textArea);

    void UpdateCompletion(bool forceOpen, bool afterDot);
    void CloseCompletion();
    void ApplyCompletion(size_t visibleIndex);
    std::string WordBeforeCaret(int& startCol) const;
    bool HasKeyboardFocus() const;

    void DrawLineText(Line& line, float x, float y, float clipLeft, float clipRight);
    void DrawSquiggle(float x0, float x1, float y, Color color);
    void DrawCompletionPopup(Rectangle area);
    void DrawDiagnosticTooltip(Rectangle area);
    Color ColorFor(TokenClass cls) const;

    std::vector<Line> lines;
    Pos caret;
    Pos anchor;
    float desiredX = 0.0f;

    float scrollX = 0.0f;
    float scrollY = 0.0f;
    bool revealCaretNext = false;
    bool draggingSelection = false;
    bool draggingVBar = false;
    bool draggingHBar = false;
    float dragBarOffset = 0.0f;

    double lastClickTime = -10.0;
    Pos lastClickPos;
    int clickCount = 0;

    int version = 0;
    float typingTimer = 0.0f;
    std::string cachedText;
    int cachedTextVersion = -1;

    std::vector<CodeDiagnostic> diagnostics;

    bool completionOpen = false;
    std::vector<CompletionItem> completionAll;
    std::vector<size_t> completionVisible;
    int completionSelected = 0;
    int completionTop = 0;
    CompletionRequest completionRequest;
    bool completionAfterDot = false;

    Rectangle lastTextArea{ 0, 0, 0, 0 };
    Rectangle lastArea{ 0, 0, 0, 0 };
    float gutterWidth = 40.0f;

    Font font{};
    float fontSize = 15.0f;
    float spacing = 1.5f;
    float lineHeight = 20.0f;
    float asciiWidth[128];
    bool metricsReady = false;
    int metricsFontId = -1;
};
