#include "CodeEditor.h"
#include "Editor/UITheme.h"
#include "Editor/EditorUndo.h"
#include "Core/KeyLayout.h"
#include "Core/Widgets/WidgetFont.h"
#include "Core/Widgets/WidgetInput.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <unordered_set>

namespace {

constexpr float kBarSize = 11.0f;
constexpr float kTextPad = 8.0f;
constexpr int kIndentWidth = 4;
constexpr float kTypingBurst = 0.8f;

bool IsWordChar(char c) {
    return isalnum((unsigned char)c) || c == '_' || ((unsigned char)c >= 0x80);
}

bool IsKeyword(const std::string& w) {
    static const std::unordered_set<std::string> set = {
        "var", "func", "event", "if", "elif", "else", "while", "for", "in", "return",
        "break", "continue", "pass", "and", "or", "not", "true", "false", "await"
    };
    return set.count(w) > 0;
}

bool IsTypeName(const std::string& w) {
    static const std::unordered_set<std::string> set = { "bool", "int", "float", "string", "Vec3", "Actor", "Array", "Component", "Hit" };
    return set.count(w) > 0;
}

bool Pressed(int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); }
bool CtrlDown() { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }
bool ShiftDown() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
bool AltDown() { return IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT); }

std::string EncodeUtf8(unsigned cp) {
    std::string out;
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    return out;
}

}

CodeEditor::CodeEditor() {
    lines.emplace_back();
    for (float& w : asciiWidth) w = 8.0f;
}

void CodeEditor::EnsureFontMetrics() {
    font = WidgetFont::Get();
    if (metricsReady && metricsFontId == (int)font.texture.id) return;
    spacing = fontSize / 10.0f;
    lineHeight = fontSize + 6.0f;
    for (int c = 0; c < 128; c++) {
        char buf[2] = { (char)c, 0 };
        if (c < 32) { asciiWidth[c] = 0.0f; continue; }
        asciiWidth[c] = MeasureTextEx(font, buf, fontSize, spacing).x + spacing;
    }
    asciiWidth[(int)'\t'] = asciiWidth[(int)' '] * kIndentWidth;
    metricsReady = true;
    metricsFontId = (int)font.texture.id;
    for (Line& l : lines) { l.metricsValid = false; }
}

float CodeEditor::CharAdvance(unsigned char c) {
    return c < 128 ? asciiWidth[c] : asciiWidth[(int)'m'];
}

void CodeEditor::EnsureMetrics(Line& line) {
    if (line.metricsValid) return;
    const std::string& s = line.text;
    line.offsets.assign(s.size() + 1, 0.0f);
    size_t i = 0;
    float x = 0.0f;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) {
            line.offsets[i] = x;
            x += asciiWidth[c];
            i++;
            line.offsets[i] = x;
        } else {
            size_t len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
            if (i + len > s.size()) len = s.size() - i;
            float w = MeasureTextEx(font, s.substr(i, len).c_str(), fontSize, spacing).x + spacing;
            for (size_t k = 0; k < len; k++) line.offsets[i + k] = x;
            x += w;
            i += len;
            line.offsets[i] = x;
        }
    }
    line.width = x;
    line.metricsValid = true;
}

void CodeEditor::EnsureSpans(Line& line) {
    if (line.spansValid) return;
    line.spans.clear();
    const std::string& s = line.text;
    size_t i = 0;
    bool afterDot = false;
    std::string prevWord;
    while (i < s.size()) {
        char c = s[i];
        if (c == ' ' || c == '\t') { i++; continue; }
        size_t start = i;
        if (c == '#') {
            line.spans.push_back({ (int)i, (int)s.size(), TokenClass::Comment });
            break;
        }
        if (c == '"' || c == '\'') {
            char q = c;
            i++;
            size_t segStart = start;
            while (i < s.size() && s[i] != q) {
                if (s[i] == '\\' && i + 1 < s.size()) { i += 2; continue; }
                if (s[i] == '{') {
                    if (i + 1 < s.size() && s[i + 1] == '{') { i += 2; continue; }
                    size_t j = i + 1;
                    int depth = 1;
                    while (j < s.size() && depth > 0) {
                        char cj = s[j];
                        if (cj == '"' || cj == '\'') {
                            char inner = cj;
                            j++;
                            while (j < s.size() && s[j] != inner) { if (s[j] == '\\') j++; j++; }
                        } else if (cj == '{') depth++;
                        else if (cj == '}') depth--;
                        j++;
                    }
                    if (depth == 0) {
                        if ((int)i > (int)segStart) line.spans.push_back({ (int)segStart, (int)i, TokenClass::String });
                        line.spans.push_back({ (int)i, (int)j, TokenClass::Variable });
                        segStart = j;
                        i = j;
                        continue;
                    }
                }
                i++;
            }
            if (i < s.size()) i++;
            if ((int)i > (int)segStart) line.spans.push_back({ (int)segStart, (int)i, TokenClass::String });
            afterDot = false;
            prevWord.clear();
            continue;
        }
        if (isdigit((unsigned char)c) || (c == '.' && i + 1 < s.size() && isdigit((unsigned char)s[i + 1]) && !afterDot)) {
            while (i < s.size() && (isdigit((unsigned char)s[i]) || s[i] == '.' || s[i] == 'e' || s[i] == 'E')) i++;
            line.spans.push_back({ (int)start, (int)i, TokenClass::Number });
            afterDot = false;
            prevWord.clear();
            continue;
        }
        if (c == '@') {
            i++;
            while (i < s.size() && IsWordChar(s[i])) i++;
            line.spans.push_back({ (int)start, (int)i, TokenClass::Annotation });
            continue;
        }
        if (IsWordChar(c)) {
            while (i < s.size() && IsWordChar(s[i])) i++;
            std::string word = s.substr(start, i - start);
            size_t look = i;
            while (look < s.size() && s[look] == ' ') look++;
            bool callFollows = look < s.size() && s[look] == '(';
            TokenClass cls = TokenClass::Plain;
            if (afterDot) {
                cls = classifyIdentifier ? classifyIdentifier(word, true, callFollows) : TokenClass::Plain;
            } else if (word == "self") cls = TokenClass::Self;
            else if (IsKeyword(word)) cls = TokenClass::Keyword;
            else if (prevWord == "func") cls = TokenClass::Function;
            else if (prevWord == "event") cls = TokenClass::Event;
            else if (IsTypeName(word)) cls = TokenClass::Type;
            else if (classifyIdentifier) cls = classifyIdentifier(word, false, callFollows);
            line.spans.push_back({ (int)start, (int)i, cls });
            prevWord = word;
            afterDot = false;
            continue;
        }
        i++;
        afterDot = (c == '.');
        if (c != '.') prevWord.clear();
    }
    line.spansValid = true;
}

float CodeEditor::XForCol(int lineIndex, int col) {
    Line& line = lines[(size_t)lineIndex];
    EnsureMetrics(line);
    col = std::max(0, std::min(col, (int)line.text.size()));
    return line.offsets[(size_t)col];
}

int CodeEditor::ColForX(int lineIndex, float x) {
    Line& line = lines[(size_t)lineIndex];
    EnsureMetrics(line);
    int n = (int)line.text.size();
    int best = 0;
    float bestDist = std::fabs(x - 0.0f);
    int i = 0;
    while (i < n) {
        int next = NextBoundary(line.text, i);
        float dist = std::fabs(x - line.offsets[(size_t)next]);
        if (dist < bestDist) { bestDist = dist; best = next; }
        if (line.offsets[(size_t)next] > x + 40.0f) break;
        i = next;
    }
    return best;
}

CodeEditor::Pos CodeEditor::PosAtPoint(Vector2 point) {
    float rel = (point.y - lastTextArea.y + scrollY) / lineHeight;
    int lineIndex = std::max(0, std::min((int)std::floor(rel), (int)lines.size() - 1));
    float x = point.x - lastTextArea.x - kTextPad + scrollX;
    return { lineIndex, ColForX(lineIndex, x) };
}

int CodeEditor::PrevBoundary(const std::string& s, int index) const {
    if (index <= 0) return 0;
    index--;
    while (index > 0 && (((unsigned char)s[(size_t)index]) & 0xC0) == 0x80) index--;
    return index;
}

int CodeEditor::NextBoundary(const std::string& s, int index) const {
    int n = (int)s.size();
    if (index >= n) return n;
    index++;
    while (index < n && (((unsigned char)s[(size_t)index]) & 0xC0) == 0x80) index++;
    return index;
}

int CodeEditor::WordLeft(Pos p) const {
    const std::string& s = lines[(size_t)p.line].text;
    int i = p.col;
    while (i > 0 && s[(size_t)i - 1] == ' ') i--;
    if (i > 0 && IsWordChar(s[(size_t)i - 1])) { while (i > 0 && IsWordChar(s[(size_t)i - 1])) i--; }
    else if (i > 0) i--;
    return i;
}

int CodeEditor::WordRight(Pos p) const {
    const std::string& s = lines[(size_t)p.line].text;
    int n = (int)s.size();
    int i = p.col;
    if (i < n && IsWordChar(s[(size_t)i])) { while (i < n && IsWordChar(s[(size_t)i])) i++; }
    else if (i < n) i++;
    while (i < n && s[(size_t)i] == ' ') i++;
    return i;
}

int CodeEditor::LeadingSpaces(const std::string& s) const {
    int n = 0;
    while (n < (int)s.size() && s[(size_t)n] == ' ') n++;
    return n;
}

CodeEditor::Pos CodeEditor::ClampPos(Pos p) const {
    p.line = std::max(0, std::min(p.line, (int)lines.size() - 1));
    p.col = std::max(0, std::min(p.col, (int)lines[(size_t)p.line].text.size()));
    return p;
}

void CodeEditor::SetText(const std::string& text) {
    lines.clear();
    std::string current;
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == '\r') continue;
        if (c == '\n') { Line l; l.text = current; lines.push_back(std::move(l)); current.clear(); continue; }
        if (c == '\t') { current += std::string(kIndentWidth, ' '); continue; }
        current += c;
    }
    Line last;
    last.text = current;
    lines.push_back(std::move(last));
    if (lines.size() > 1 && lines.back().text.empty()) lines.pop_back();
    caret = ClampPos(caret);
    anchor = ClampPos(anchor);
    version++;
    cachedTextVersion = -1;
    CloseCompletion();
}

const std::string& CodeEditor::GetText() {
    if (cachedTextVersion == version) return cachedText;
    cachedText.clear();
    for (size_t i = 0; i < lines.size(); i++) {
        cachedText += lines[i].text;
        cachedText += '\n';
    }
    cachedTextVersion = version;
    return cachedText;
}

std::string CodeEditor::GetViewState() const {
    std::ostringstream ss;
    ss << caret.line << "," << caret.col << "," << anchor.line << "," << anchor.col << "," << (int)scrollY << "," << (int)scrollX;
    return ss.str();
}

void CodeEditor::SetViewState(const std::string& state) {
    int v[6] = { 0, 0, 0, 0, 0, 0 };
    std::stringstream ss(state);
    std::string token;
    int n = 0;
    while (n < 6 && std::getline(ss, token, ',')) v[n++] = atoi(token.c_str());
    if (n < 4) return;
    caret = ClampPos({ v[0], v[1] });
    anchor = ClampPos({ v[2], v[3] });
    if (n >= 5) scrollY = (float)v[4];
    if (n >= 6) scrollX = (float)v[5];
    CloseCompletion();
}

void CodeEditor::GotoLine(int line, int column) {
    caret = ClampPos({ line, column });
    anchor = caret;
    revealCaretNext = true;
    float areaH = lastTextArea.height;
    if (areaH > 0) scrollY = std::max(0.0f, caret.line * lineHeight - areaH * 0.35f);
    CloseCompletion();
}

void CodeEditor::AppendText(const std::string& text) {
    Pos end = { (int)lines.size() - 1, (int)lines.back().text.size() };
    ReplaceRange(end, end, text);
}

void CodeEditor::SelectRange(int line, int column, int length) {
    anchor = ClampPos({ line, column });
    caret = ClampPos({ line, column + length });
    revealCaretNext = true;
    desiredX = XForCol(caret.line, caret.col);
}

bool CodeEditor::InsertTextAtPoint(Vector2 point, const std::string& text) {
    if (!CheckCollisionPointRec(point, lastTextArea)) return false;
    Pos target = PosAtPoint(point);
    caret = target;
    anchor = target;
    InsertText(text);
    revealCaretNext = true;
    return true;
}

int CodeEditor::SelectionLength() const {
    if (!HasSelection()) return 0;
    return (int)TextInRange(SelStart(), SelEnd()).size();
}

void CodeEditor::SetDiagnostics(std::vector<CodeDiagnostic> newDiagnostics) {
    diagnostics = std::move(newDiagnostics);
}

std::string CodeEditor::TextInRange(Pos a, Pos b) const {
    if (b < a) std::swap(a, b);
    if (a.line == b.line) return lines[(size_t)a.line].text.substr((size_t)a.col, (size_t)(b.col - a.col));
    std::string out = lines[(size_t)a.line].text.substr((size_t)a.col);
    for (int l = a.line + 1; l < b.line; l++) { out += '\n'; out += lines[(size_t)l].text; }
    out += '\n';
    out += lines[(size_t)b.line].text.substr(0, (size_t)b.col);
    return out;
}

void CodeEditor::MarkChanged() {
    version++;
    typingTimer = kTypingBurst;
    if (onTextChanged) onTextChanged();
}

void CodeEditor::ReplaceRange(Pos a, Pos b, const std::string& text) {
    if (b < a) std::swap(a, b);
    a = ClampPos(a);
    b = ClampPos(b);

    std::string head = lines[(size_t)a.line].text.substr(0, (size_t)a.col);
    std::string tail = lines[(size_t)b.line].text.substr((size_t)b.col);

    std::vector<std::string> pieces;
    std::string cur;
    for (char c : text) {
        if (c == '\r') continue;
        if (c == '\n') { pieces.push_back(cur); cur.clear(); continue; }
        if (c == '\t') { cur += std::string(kIndentWidth, ' '); continue; }
        cur += c;
    }
    pieces.push_back(cur);

    std::vector<Line> replacement;
    for (size_t i = 0; i < pieces.size(); i++) {
        Line l;
        l.text = pieces[i];
        if (i == 0) l.text = head + l.text;
        if (i + 1 == pieces.size()) {
            caret = { a.line + (int)i, (int)l.text.size() };
            l.text += tail;
        }
        replacement.push_back(std::move(l));
    }

    lines.erase(lines.begin() + a.line, lines.begin() + b.line + 1);
    lines.insert(lines.begin() + a.line, replacement.begin(), replacement.end());
    anchor = caret;
    revealCaretNext = true;
    MarkChanged();
}

void CodeEditor::InsertText(const std::string& text) {
    ReplaceRange(SelStart(), SelEnd(), text);
    desiredX = XForCol(caret.line, caret.col);
}

void CodeEditor::InsertChar(unsigned cp) {
    if (cp < 32 || cp == 127) return;
    std::string ch = EncodeUtf8(cp);
    const std::string& s = lines[(size_t)caret.line].text;

    auto closerFor = [](char open) -> char {
        switch (open) { case '(': return ')'; case '[': return ']'; case '"': return '"'; case '\'': return '\''; default: return 0; }
    };

    if (!HasSelection() && ch.size() == 1) {
        char c = ch[0];
        if ((c == ')' || c == ']' || c == '"' || c == '\'') && caret.col < (int)s.size() && s[(size_t)caret.col] == c) {
            caret.col++;
            anchor = caret;
            revealCaretNext = true;
            CloseCompletion();
            return;
        }
    }

    if (HasSelection() && ch.size() == 1 && closerFor(ch[0])) {
        std::string selected = TextInRange(SelStart(), SelEnd());
        if (selected.find('\n') == std::string::npos) {
            Pos start = SelStart();
            InsertText(std::string(1, ch[0]) + selected + std::string(1, closerFor(ch[0])));
            anchor = { start.line, start.col + 1 };
            caret = { start.line, start.col + 1 + (int)selected.size() };
            return;
        }
    }

    if (!HasSelection() && ch.size() == 1 && closerFor(ch[0])) {
        char open = ch[0];
        bool nextIsWord = caret.col < (int)s.size() && IsWordChar(s[(size_t)caret.col]);
        bool prevIsWord = caret.col > 0 && IsWordChar(s[(size_t)caret.col - 1]);
        bool quote = open == '"' || open == '\'';
        if (!nextIsWord && !(quote && prevIsWord)) {
            InsertText(std::string(1, open) + std::string(1, closerFor(open)));
            caret.col--;
            anchor = caret;
            CloseCompletion();
            return;
        }
    }

    InsertText(ch);

    bool wordChar = ch.size() > 1 || IsWordChar(ch[0]);
    if (ch == ".") UpdateCompletion(true, true);
    else if (wordChar) UpdateCompletion(completionOpen ? false : true, false);
    else CloseCompletion();
}

void CodeEditor::DeleteBackward(bool wholeWord) {
    if (HasSelection()) { InsertText(""); CloseCompletion(); return; }
    if (caret.col == 0) {
        if (caret.line == 0) return;
        Pos from = { caret.line - 1, (int)lines[(size_t)caret.line - 1].text.size() };
        ReplaceRange(from, caret, "");
        return;
    }
    const std::string& s = lines[(size_t)caret.line].text;
    int target = wholeWord ? WordLeft(caret) : PrevBoundary(s, caret.col);

    if (!wholeWord && caret.col < (int)s.size()) {
        char before = s[(size_t)caret.col - 1], after = s[(size_t)caret.col];
        if ((before == '(' && after == ')') || (before == '[' && after == ']') || (before == '"' && after == '"') || (before == '\'' && after == '\'')) {
            ReplaceRange({ caret.line, caret.col - 1 }, { caret.line, caret.col + 1 }, "");
            if (completionOpen) UpdateCompletion(false, false);
            return;
        }
    }
    ReplaceRange({ caret.line, target }, caret, "");
    desiredX = XForCol(caret.line, caret.col);
    if (completionOpen) UpdateCompletion(false, completionAfterDot);
}

void CodeEditor::DeleteForward(bool wholeWord) {
    if (HasSelection()) { InsertText(""); CloseCompletion(); return; }
    const std::string& s = lines[(size_t)caret.line].text;
    if (caret.col >= (int)s.size()) {
        if (caret.line + 1 >= (int)lines.size()) return;
        ReplaceRange(caret, { caret.line + 1, 0 }, "");
        return;
    }
    int target = wholeWord ? WordRight(caret) : NextBoundary(s, caret.col);
    ReplaceRange(caret, { caret.line, target }, "");
}

void CodeEditor::NewLineWithIndent() {
    if (completionOpen) CloseCompletion();
    Pos start = SelStart();
    const std::string& s = lines[(size_t)start.line].text;
    int indent = LeadingSpaces(s);
    std::string before = s.substr(0, (size_t)start.col);
    size_t comment = before.find('#');
    if (comment != std::string::npos) before = before.substr(0, comment);
    while (!before.empty() && before.back() == ' ') before.pop_back();
    if (!before.empty() && before.back() == ':') indent += kIndentWidth;
    if (start.col < indent && start.col < LeadingSpaces(s)) indent = start.col;
    InsertText("\n" + std::string((size_t)indent, ' '));
}

void CodeEditor::IndentSelection(bool outdent) {
    Pos a = SelStart(), b = SelEnd();
    int first = a.line, last = b.line;
    if (b.col == 0 && last > first) last--;
    Pos savedA = anchor, savedC = caret;
    bool anchorFirst = anchor < caret;
    std::vector<int> shift((size_t)(last - first + 1), 0);
    for (int l = first; l <= last; l++) {
        std::string& t = lines[(size_t)l].text;
        if (!outdent) {
            if (t.empty()) continue;
            t.insert(0, std::string(kIndentWidth, ' '));
            shift[(size_t)(l - first)] = kIndentWidth;
        } else {
            int remove = std::min(kIndentWidth, LeadingSpaces(t));
            if (remove > 0) t.erase(0, (size_t)remove);
            shift[(size_t)(l - first)] = -remove;
        }
        lines[(size_t)l].metricsValid = false;
        lines[(size_t)l].spansValid = false;
    }
    auto adjust = [&](Pos p) {
        if (p.line >= first && p.line <= last) p.col = std::max(0, p.col + shift[(size_t)(p.line - first)]);
        return p;
    };
    anchor = adjust(savedA);
    caret = adjust(savedC);
    (void)anchorFirst;
    revealCaretNext = true;
    MarkChanged();
}

void CodeEditor::ToggleComment() {
    Pos a = SelStart(), b = SelEnd();
    int first = a.line, last = b.line;
    if (b.col == 0 && last > first) last--;
    bool allCommented = true;
    for (int l = first; l <= last; l++) {
        const std::string& t = lines[(size_t)l].text;
        if (t.find_first_not_of(' ') == std::string::npos) continue;
        if (t[(size_t)LeadingSpaces(t)] != '#') { allCommented = false; break; }
    }
    int minIndent = 1 << 20;
    for (int l = first; l <= last; l++) {
        const std::string& t = lines[(size_t)l].text;
        if (t.find_first_not_of(' ') == std::string::npos) continue;
        minIndent = std::min(minIndent, LeadingSpaces(t));
    }
    if (minIndent == (1 << 20)) minIndent = 0;
    Pos savedA = anchor, savedC = caret;
    std::vector<int> shift((size_t)(last - first + 1), 0);
    for (int l = first; l <= last; l++) {
        std::string& t = lines[(size_t)l].text;
        if (t.find_first_not_of(' ') == std::string::npos) continue;
        if (allCommented) {
            int at = LeadingSpaces(t);
            int remove = (at + 1 < (int)t.size() && t[(size_t)at + 1] == ' ') ? 2 : 1;
            t.erase((size_t)at, (size_t)remove);
            shift[(size_t)(l - first)] = -remove;
        } else {
            t.insert((size_t)minIndent, "# ");
            shift[(size_t)(l - first)] = 2;
        }
        lines[(size_t)l].metricsValid = false;
        lines[(size_t)l].spansValid = false;
    }
    auto adjust = [&](Pos p) {
        if (p.line >= first && p.line <= last) p.col = std::max(0, p.col + shift[(size_t)(p.line - first)]);
        return p;
    };
    anchor = adjust(savedA);
    caret = adjust(savedC);
    MarkChanged();
}

void CodeEditor::DuplicateLines() {
    if (HasSelection()) {
        std::string text = TextInRange(SelStart(), SelEnd());
        Pos end = SelEnd();
        caret = end;
        anchor = end;
        InsertText(text);
        return;
    }
    Pos saved = caret;
    std::string text = lines[(size_t)caret.line].text;
    Pos endOfLine = { caret.line, (int)text.size() };
    ReplaceRange(endOfLine, endOfLine, "\n" + text);
    caret = { saved.line + 1, saved.col };
    anchor = caret;
}

void CodeEditor::MoveLines(int direction) {
    Pos a = SelStart(), b = SelEnd();
    int first = a.line, last = b.line;
    if (b.col == 0 && last > first) last--;
    if (direction < 0 && first == 0) return;
    if (direction > 0 && last + 1 >= (int)lines.size()) return;
    if (direction < 0) {
        Line moved = std::move(lines[(size_t)first - 1]);
        lines.erase(lines.begin() + first - 1);
        lines.insert(lines.begin() + last, std::move(moved));
    } else {
        Line moved = std::move(lines[(size_t)last + 1]);
        lines.erase(lines.begin() + last + 1);
        lines.insert(lines.begin() + first, std::move(moved));
    }
    anchor.line += direction;
    caret.line += direction;
    revealCaretNext = true;
    MarkChanged();
}

void CodeEditor::DeleteLines() {
    Pos a = SelStart(), b = SelEnd();
    int first = a.line, last = b.line;
    if (b.col == 0 && last > first) last--;
    if (lines.size() == 1) { ReplaceRange({ 0, 0 }, { 0, (int)lines[0].text.size() }, ""); return; }
    lines.erase(lines.begin() + first, lines.begin() + last + 1);
    if (lines.empty()) lines.emplace_back();
    caret = ClampPos({ first, caret.col });
    anchor = caret;
    revealCaretNext = true;
    MarkChanged();
}

void CodeEditor::SelectAll() {
    anchor = { 0, 0 };
    caret = { (int)lines.size() - 1, (int)lines.back().text.size() };
}

void CodeEditor::SelectWordAt(Pos p) {
    const std::string& s = lines[(size_t)p.line].text;
    int start = p.col, end = p.col;
    if (start < (int)s.size() && IsWordChar(s[(size_t)start])) {
        while (start > 0 && IsWordChar(s[(size_t)start - 1])) start--;
        while (end < (int)s.size() && IsWordChar(s[(size_t)end])) end++;
    } else if (start > 0 && IsWordChar(s[(size_t)start - 1])) {
        end = start;
        while (start > 0 && IsWordChar(s[(size_t)start - 1])) start--;
    } else if (start < (int)s.size()) {
        end = NextBoundary(s, start);
    }
    anchor = { p.line, start };
    caret = { p.line, end };
}

void CodeEditor::MoveCaret(Pos target, bool extend, bool keepDesiredX) {
    caret = ClampPos(target);
    if (!extend) anchor = caret;
    if (!keepDesiredX) desiredX = XForCol(caret.line, caret.col);
    revealCaretNext = true;
}

void CodeEditor::MoveVertical(int deltaLines, bool extend) {
    int target = caret.line + deltaLines;
    if (target < 0) { MoveCaret({ 0, 0 }, extend); return; }
    if (target >= (int)lines.size()) { MoveCaret({ (int)lines.size() - 1, (int)lines.back().text.size() }, extend); return; }
    MoveCaret({ target, ColForX(target, desiredX) }, extend, true);
}

bool CodeEditor::HasKeyboardFocus() const {
    const void* scope = WidgetInput::GetInputScope();
    if (!scope) return true;
    if (WidgetInput::HasModal(WidgetInput::kEditorDevice)) return false;
    const void* owner = WidgetInput::GetFocusOwner(WidgetInput::kEditorDevice);
    return owner == nullptr || owner == scope;
}

void CodeEditor::HandleKeyboard() {
    const bool ctrl = CtrlDown();
    const bool shift = ShiftDown();
    const bool alt = AltDown();

    if (completionOpen) {
        if (Pressed(KEY_DOWN)) { completionSelected = std::min(completionSelected + 1, (int)completionVisible.size() - 1); if (completionSelected >= completionTop + 8) completionTop = completionSelected - 7; return; }
        if (Pressed(KEY_UP)) { completionSelected = std::max(completionSelected - 1, 0); if (completionSelected < completionTop) completionTop = completionSelected; return; }
        if (IsKeyPressed(KEY_ESCAPE)) { CloseCompletion(); return; }
        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            ApplyCompletion((size_t)completionSelected);
            return;
        }
    }

    if (ctrl && !alt) {
        if (KeyLayout::PressedOrRepeat('z')) {
            typingTimer = 0.0f;
            if (shift) EditorUndo::PerformRedo(); else EditorUndo::PerformUndo();
            return;
        }
        if (KeyLayout::PressedOrRepeat('y')) { typingTimer = 0.0f; EditorUndo::PerformRedo(); return; }
        if (KeyLayout::Pressed('a')) { SelectAll(); return; }
        if (KeyLayout::Pressed('s')) { if (onSaveRequested) onSaveRequested(); return; }
        if (KeyLayout::Pressed('c') || KeyLayout::Pressed('x')) {
            bool cut = KeyLayout::Pressed('x');
            std::string text;
            Pos a = SelStart(), b = SelEnd();
            if (HasSelection()) text = TextInRange(a, b);
            else text = lines[(size_t)caret.line].text + "\n";
            SetClipboardText(text.c_str());
            if (cut) {
                if (HasSelection()) InsertText("");
                else DeleteLines();
            }
            return;
        }
        if (KeyLayout::PressedOrRepeat('v')) {
            const char* clip = GetClipboardText();
            if (clip && *clip) { CloseCompletion(); InsertText(clip); }
            return;
        }
        if (KeyLayout::Pressed('d')) { DuplicateLines(); return; }
        if (KeyLayout::Pressed('k') && shift) { DeleteLines(); return; }
        if (IsKeyPressed(KEY_SLASH) || IsKeyPressed(KEY_KP_DIVIDE)) { ToggleComment(); return; }
        if (IsKeyPressed(KEY_SPACE)) { UpdateCompletion(true, false); return; }
        if (Pressed(KEY_LEFT)) { MoveCaret({ caret.line, WordLeft(caret) }, shift); CloseCompletion(); return; }
        if (Pressed(KEY_RIGHT)) { MoveCaret({ caret.line, WordRight(caret) }, shift); CloseCompletion(); return; }
        if (IsKeyPressed(KEY_HOME)) { MoveCaret({ 0, 0 }, shift); return; }
        if (IsKeyPressed(KEY_END)) { MoveCaret({ (int)lines.size() - 1, (int)lines.back().text.size() }, shift); return; }
        if (Pressed(KEY_BACKSPACE)) { DeleteBackward(true); return; }
        if (Pressed(KEY_DELETE)) { DeleteForward(true); return; }
        return;
    }

    if (alt && Pressed(KEY_UP)) { MoveLines(-1); return; }
    if (alt && Pressed(KEY_DOWN)) { MoveLines(1); return; }

    if (Pressed(KEY_LEFT)) {
        if (HasSelection() && !shift) MoveCaret(SelStart(), false);
        else if (caret.col > 0) MoveCaret({ caret.line, PrevBoundary(lines[(size_t)caret.line].text, caret.col) }, shift);
        else if (caret.line > 0) MoveCaret({ caret.line - 1, (int)lines[(size_t)caret.line - 1].text.size() }, shift);
        CloseCompletion();
        return;
    }
    if (Pressed(KEY_RIGHT)) {
        if (HasSelection() && !shift) MoveCaret(SelEnd(), false);
        else if (caret.col < (int)lines[(size_t)caret.line].text.size()) MoveCaret({ caret.line, NextBoundary(lines[(size_t)caret.line].text, caret.col) }, shift);
        else if (caret.line + 1 < (int)lines.size()) MoveCaret({ caret.line + 1, 0 }, shift);
        CloseCompletion();
        return;
    }
    if (Pressed(KEY_UP)) { MoveVertical(-1, shift); return; }
    if (Pressed(KEY_DOWN)) { MoveVertical(1, shift); return; }
    if (Pressed(KEY_PAGE_UP)) { MoveVertical(-std::max(1, (int)(lastTextArea.height / lineHeight) - 1), shift); return; }
    if (Pressed(KEY_PAGE_DOWN)) { MoveVertical(std::max(1, (int)(lastTextArea.height / lineHeight) - 1), shift); return; }
    if (IsKeyPressed(KEY_HOME)) {
        int firstText = LeadingSpaces(lines[(size_t)caret.line].text);
        MoveCaret({ caret.line, caret.col == firstText ? 0 : firstText }, shift);
        return;
    }
    if (IsKeyPressed(KEY_END)) { MoveCaret({ caret.line, (int)lines[(size_t)caret.line].text.size() }, shift); return; }
    if (Pressed(KEY_BACKSPACE)) { DeleteBackward(false); return; }
    if (Pressed(KEY_DELETE)) { DeleteForward(false); return; }
    if (Pressed(KEY_ENTER) || Pressed(KEY_KP_ENTER)) { NewLineWithIndent(); return; }
    if (Pressed(KEY_TAB)) {
        if (shift) { IndentSelection(true); return; }
        if (HasSelection() && SelStart().line != SelEnd().line) { IndentSelection(false); return; }
        int pad = kIndentWidth - (caret.col % kIndentWidth);
        InsertText(std::string((size_t)pad, ' '));
        return;
    }

    int cp;
    while ((cp = GetCharPressed()) != 0) InsertChar((unsigned)cp);
}

void CodeEditor::ClampScroll(Rectangle textArea) {
    float contentH = lines.size() * lineHeight + lineHeight * 2.0f;
    float maxY = std::max(0.0f, contentH - textArea.height);
    scrollY = std::max(0.0f, std::min(scrollY, maxY));
    float contentW = kTextPad * 2.0f + 40.0f;
    for (Line& l : lines) { EnsureMetrics(l); contentW = std::max(contentW, l.width + kTextPad * 2.0f + 40.0f); }
    float maxX = std::max(0.0f, contentW - textArea.width);
    scrollX = std::max(0.0f, std::min(scrollX, maxX));
}

void CodeEditor::RevealCaret(Rectangle textArea) {
    float caretY = caret.line * lineHeight;
    if (caretY < scrollY) scrollY = caretY;
    else if (caretY + lineHeight > scrollY + textArea.height) scrollY = caretY + lineHeight - textArea.height;
    float caretX = XForCol(caret.line, caret.col) + kTextPad;
    if (caretX < scrollX + 20.0f) scrollX = std::max(0.0f, caretX - 40.0f);
    else if (caretX > scrollX + textArea.width - 20.0f) scrollX = caretX - textArea.width + 40.0f;
}

void CodeEditor::HandleMouse(Rectangle textArea, Rectangle vBar, Rectangle hBar) {
    Vector2 mouse = GetMousePosition();
    const void* scope = WidgetInput::GetInputScope();
    bool blocked = WidgetInput::IsMouseBlocked() || (scope && WidgetInput::IsMouseOccluded(mouse, scope));
    bool overText = !blocked && CheckCollisionPointRec(mouse, textArea);

    float contentH = lines.size() * lineHeight + lineHeight * 2.0f;
    float contentW = 0.0f;
    for (Line& l : lines) { EnsureMetrics(l); contentW = std::max(contentW, l.width + kTextPad * 2.0f + 40.0f); }

    if (overText || (!blocked && CheckCollisionPointRec(mouse, vBar)) || (!blocked && CheckCollisionPointRec(mouse, hBar))) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            if (ShiftDown()) scrollX -= wheel * 60.0f;
            else scrollY -= wheel * lineHeight * 3.0f;
        }
    }

    float vTrack = vBar.height, hTrack = hBar.width;
    float vThumbH = std::max(24.0f, vTrack * std::min(1.0f, textArea.height / contentH));
    float hThumbW = std::max(24.0f, hTrack * std::min(1.0f, textArea.width / std::max(contentW, 1.0f)));
    float maxY = std::max(1.0f, contentH - textArea.height), maxX = std::max(1.0f, contentW - textArea.width);
    float vThumbY = vBar.y + (vTrack - vThumbH) * (scrollY / maxY);
    float hThumbX = hBar.x + (hTrack - hThumbW) * (scrollX / maxX);
    Rectangle vThumb = { vBar.x, vThumbY, vBar.width, vThumbH };
    Rectangle hThumb = { hThumbX, hBar.y, hThumbW, hBar.height };

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !blocked) {
        if (contentH > textArea.height && CheckCollisionPointRec(mouse, vBar)) {
            draggingVBar = true;
            dragBarOffset = CheckCollisionPointRec(mouse, vThumb) ? mouse.y - vThumbY : vThumbH * 0.5f;
        } else if (contentW > textArea.width && CheckCollisionPointRec(mouse, hBar)) {
            draggingHBar = true;
            dragBarOffset = CheckCollisionPointRec(mouse, hThumb) ? mouse.x - hThumbX : hThumbW * 0.5f;
        } else if (overText) {
            Pos p = PosAtPoint(mouse);
            double now = GetTime();
            if (now - lastClickTime < 0.4 && p.line == lastClickPos.line && std::abs(p.col - lastClickPos.col) <= 1) clickCount++;
            else clickCount = 1;
            lastClickTime = now;
            lastClickPos = p;
            CloseCompletion();
            if (clickCount == 2) SelectWordAt(p);
            else if (clickCount >= 3) {
                anchor = { p.line, 0 };
                caret = { p.line, (int)lines[(size_t)p.line].text.size() };
                clickCount = 0;
            } else {
                caret = p;
                if (!ShiftDown()) anchor = p;
                draggingSelection = true;
            }
            desiredX = XForCol(caret.line, caret.col);
        }
    }

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { draggingSelection = false; draggingVBar = false; draggingHBar = false; }

    if (draggingVBar) {
        float t = (mouse.y - vBar.y - dragBarOffset) / std::max(1.0f, vTrack - vThumbH);
        scrollY = t * maxY;
    }
    if (draggingHBar) {
        float t = (mouse.x - hBar.x - dragBarOffset) / std::max(1.0f, hTrack - hThumbW);
        scrollX = t * maxX;
    }
    if (draggingSelection) {
        if (mouse.y < textArea.y) scrollY -= (textArea.y - mouse.y) * 0.3f + 2.0f;
        else if (mouse.y > textArea.y + textArea.height) scrollY += (mouse.y - textArea.y - textArea.height) * 0.3f + 2.0f;
        if (mouse.x < textArea.x) scrollX -= 6.0f;
        else if (mouse.x > textArea.x + textArea.width) scrollX += 6.0f;
        caret = PosAtPoint(mouse);
        desiredX = XForCol(caret.line, caret.col);
    }
}

std::string CodeEditor::WordBeforeCaret(int& startCol) const {
    const std::string& s = lines[(size_t)caret.line].text;
    int i = caret.col;
    while (i > 0 && IsWordChar(s[(size_t)i - 1])) i--;
    startCol = i;
    return s.substr((size_t)i, (size_t)(caret.col - i));
}

void CodeEditor::CloseCompletion() {
    completionOpen = false;
    completionAll.clear();
    completionVisible.clear();
    completionSelected = 0;
    completionTop = 0;
}

void CodeEditor::UpdateCompletion(bool forceOpen, bool afterDot) {
    if (!completionProvider) return;
    if (forceOpen || !completionOpen) {
        completionRequest = CompletionRequest();
        completionRequest.afterDot = false;
        int startCol = 0;
        WordBeforeCaret(startCol);
        const std::string& s = lines[(size_t)caret.line].text;
        int dotCol = startCol - 1;
        if (dotCol >= 0 && s[(size_t)dotCol] == '.') {
            completionRequest.afterDot = true;
            int end = dotCol;
            int begin = end;
            while (begin > 0 && (IsWordChar(s[(size_t)begin - 1]) || s[(size_t)begin - 1] == '.')) begin--;
            completionRequest.receiver = s.substr((size_t)begin, (size_t)(end - begin));
        }
        {
            size_t first = s.find_first_not_of(" \t");
            completionRequest.inEventHeader = first != std::string::npos && s.compare(first, 6, "event ") == 0;
        }
        completionAfterDot = completionRequest.afterDot;
        completionAll.clear();
        completionProvider(completionRequest, completionAll);
        completionOpen = true;
    }

    int startCol = 0;
    std::string prefix = WordBeforeCaret(startCol);
    if (prefix.empty() && !completionAfterDot) { CloseCompletion(); return; }

    std::string lowerPrefix = prefix;
    for (char& c : lowerPrefix) c = (char)tolower((unsigned char)c);

    std::vector<size_t> exact, ci, contains;
    for (size_t i = 0; i < completionAll.size(); i++) {
        const std::string& label = completionAll[i].label;
        if (prefix.empty()) { exact.push_back(i); continue; }
        std::string lower = label;
        for (char& c : lower) c = (char)tolower((unsigned char)c);
        if (label.compare(0, prefix.size(), prefix) == 0) exact.push_back(i);
        else if (lower.compare(0, lowerPrefix.size(), lowerPrefix) == 0) ci.push_back(i);
        else if (lower.find(lowerPrefix) != std::string::npos) contains.push_back(i);
    }
    completionVisible.clear();
    for (auto* group : { &exact, &ci, &contains }) for (size_t i : *group) completionVisible.push_back(i);
    if (completionVisible.size() == 1 && completionAll[completionVisible[0]].label == prefix) completionVisible.clear();
    if (completionVisible.empty()) { CloseCompletion(); return; }
    if (completionSelected >= (int)completionVisible.size()) completionSelected = 0;
    completionTop = std::min(completionTop, std::max(0, (int)completionVisible.size() - 1));
    (void)afterDot;
}

void CodeEditor::ApplyCompletion(size_t visibleIndex) {
    if (visibleIndex >= completionVisible.size()) { CloseCompletion(); return; }
    CompletionItem item = completionAll[completionVisible[visibleIndex]];
    int startCol = 0;
    WordBeforeCaret(startCol);
    std::string text = item.insertText.empty() ? item.label : item.insertText;
    int caretBack = -1;
    size_t marker = text.find('|');
    if (marker != std::string::npos) { caretBack = (int)(text.size() - marker - 1); text.erase(marker, 1); }
    ReplaceRange({ caret.line, startCol }, caret, text);
    if (caretBack >= 0) { caret.col -= caretBack; anchor = caret; }
    desiredX = XForCol(caret.line, caret.col);
    CloseCompletion();
}

Color CodeEditor::ColorFor(TokenClass cls) const {
    switch (cls) {
        case TokenClass::Keyword: return { 197, 134, 192, 255 };
        case TokenClass::Type: return { 78, 201, 176, 255 };
        case TokenClass::Builtin: return { 220, 220, 170, 255 };
        case TokenClass::Function: return { 220, 220, 170, 255 };
        case TokenClass::Variable: return { 156, 220, 254, 255 };
        case TokenClass::Component: return { 78, 201, 176, 255 };
        case TokenClass::Number: return { 181, 206, 168, 255 };
        case TokenClass::String: return { 206, 145, 120, 255 };
        case TokenClass::Comment: return { 106, 153, 85, 255 };
        case TokenClass::Annotation: return { 86, 156, 214, 255 };
        case TokenClass::Event: return { 255, 180, 100, 255 };
        case TokenClass::Self: return { 86, 156, 214, 255 };
        default: return { 212, 212, 212, 255 };
    }
}

void CodeEditor::DrawSquiggle(float x0, float x1, float y, Color color) {
    float x = x0;
    bool up = false;
    while (x < x1) {
        float nx = std::min(x + 2.0f, x1);
        DrawLineEx({ x, y + (up ? -1.5f : 1.5f) }, { nx, y + (up ? 1.5f : -1.5f) }, 1.0f, color);
        x = nx;
        up = !up;
    }
}

void CodeEditor::DrawLineText(Line& line, float x, float y, float clipLeft, float clipRight) {
    EnsureSpans(line);
    EnsureMetrics(line);
    const std::string& s = line.text;
    size_t pos = 0;
    auto drawSegment = [&](int from, int to, Color color) {
        if (to <= from) return;
        float sx = x + line.offsets[(size_t)from];
        float ex = x + line.offsets[(size_t)to];
        if (ex < clipLeft || sx > clipRight) return;
        std::string seg = s.substr((size_t)from, (size_t)(to - from));
        for (char& c : seg) if (c == '\t') c = ' ';
        DrawTextEx(font, seg.c_str(), { sx, y }, fontSize, spacing, color);
    };
    for (const Span& span : line.spans) {
        if ((int)pos < span.start) drawSegment((int)pos, span.start, ColorFor(TokenClass::Plain));
        drawSegment(span.start, span.end, ColorFor(span.cls));
        pos = (size_t)span.end;
    }
    if (pos < s.size()) drawSegment((int)pos, (int)s.size(), ColorFor(TokenClass::Plain));
}

void CodeEditor::Render(Rectangle area) {
    EnsureFontMetrics();
    const UIPalette& theme = UITheme::Get();
    lastArea = area;

    int digits = std::max(3, (int)std::to_string(lines.size()).size());
    gutterWidth = digits * asciiWidth[(int)'0'] + 24.0f;

    float contentH = lines.size() * lineHeight + lineHeight * 2.0f;
    float contentWTmp = 0.0f;
    for (Line& l : lines) { EnsureMetrics(l); contentWTmp = std::max(contentWTmp, l.width + kTextPad * 2.0f + 40.0f); }

    bool needV = contentH > area.height - kBarSize + 0.5f || contentH > area.height;
    bool needH = contentWTmp > area.width - gutterWidth - (needV ? kBarSize : 0.0f);
    float vBarW = needV ? kBarSize : 0.0f;
    float hBarH = needH ? kBarSize : 0.0f;

    Rectangle gutter = { area.x, area.y, gutterWidth, area.height - hBarH };
    Rectangle textArea = { area.x + gutterWidth, area.y, area.width - gutterWidth - vBarW, area.height - hBarH };
    Rectangle vBar = { area.x + area.width - kBarSize, area.y, kBarSize, area.height - hBarH };
    Rectangle hBar = { textArea.x, area.y + area.height - kBarSize, textArea.width, kBarSize };
    lastTextArea = textArea;

    const bool focused = HasKeyboardFocus();
    if (typingTimer > 0.0f) {
        typingTimer -= GetFrameTime();
        WidgetInput::MarkKeyboardCaptured();
    }

    Pos caretBefore = caret;
    HandleMouse(textArea, vBar, hBar);
    if (focused && !WidgetInput::IsMouseBlocked()) HandleKeyboard();
    if (caret != caretBefore && !HasSelection()) {}

    if (revealCaretNext) { RevealCaret(textArea); revealCaretNext = false; }
    ClampScroll(textArea);

    Color bg = { 30, 30, 34, 255 };
    Color gutterBg = { 26, 26, 30, 255 };
    DrawRectangleRec(area, bg);
    DrawRectangleRec(gutter, gutterBg);

    int firstLine = std::max(0, (int)std::floor(scrollY / lineHeight));
    int lastLine = std::min((int)lines.size() - 1, (int)std::floor((scrollY + textArea.height) / lineHeight));

    BeginScissorMode((int)textArea.x, (int)textArea.y, (int)textArea.width, (int)textArea.height);

    Pos selA = SelStart(), selB = SelEnd();
    for (int l = firstLine; l <= lastLine; l++) {
        float y = textArea.y + l * lineHeight - scrollY;
        if (l == caret.line && !HasSelection()) DrawRectangle((int)textArea.x, (int)y, (int)textArea.width, (int)lineHeight, { 255, 255, 255, 12 });
        if (HasSelection() && l >= selA.line && l <= selB.line) {
            Line& line = lines[(size_t)l];
            EnsureMetrics(line);
            int c0 = l == selA.line ? selA.col : 0;
            int c1 = l == selB.line ? selB.col : (int)line.text.size();
            float x0 = textArea.x + kTextPad - scrollX + line.offsets[(size_t)c0];
            float x1 = textArea.x + kTextPad - scrollX + line.offsets[(size_t)c1];
            if (l != selB.line) x1 += asciiWidth[(int)' '] * 0.6f;
            DrawRectangle((int)x0, (int)y, std::max(2, (int)(x1 - x0)), (int)lineHeight, { 38, 79, 120, 255 });
        }
    }

    for (int l = firstLine; l <= lastLine; l++) {
        Line& line = lines[(size_t)l];
        EnsureMetrics(line);
        float y = textArea.y + l * lineHeight - scrollY;
        float x = textArea.x + kTextPad - scrollX;

        int indent = LeadingSpaces(line.text);
        for (int g = kIndentWidth; g <= indent; g += kIndentWidth) {
            float gx = x + line.offsets[(size_t)(g - 0)] - 1.0f;
            if (g - 1 >= 0 && g <= (int)line.text.size()) DrawLine((int)gx, (int)y, (int)gx, (int)(y + lineHeight), { 255, 255, 255, 18 });
        }

        DrawLineText(line, x, y + (lineHeight - fontSize) * 0.5f, textArea.x, textArea.x + textArea.width);

        for (const CodeDiagnostic& d : diagnostics) {
            if (d.line != l) continue;
            int col = std::min(d.column, (int)line.text.size());
            int end = col;
            if (end < (int)line.text.size()) {
                if (IsWordChar(line.text[(size_t)end])) { while (end < (int)line.text.size() && IsWordChar(line.text[(size_t)end])) end++; }
                else end = NextBoundary(line.text, end);
            } else if (col > 0) { col = PrevBoundary(line.text, col); }
            if (end == col && line.text.empty()) { DrawSquiggle(x, x + 20.0f, y + lineHeight - 3.0f, { 244, 71, 71, 255 }); continue; }
            DrawSquiggle(x + line.offsets[(size_t)col], x + line.offsets[(size_t)end], y + lineHeight - 3.0f, { 244, 71, 71, 255 });
        }
    }

    if (focused && fmod(GetTime(), 1.0) < 0.6) {
        Line& line = lines[(size_t)caret.line];
        EnsureMetrics(line);
        float cx = textArea.x + kTextPad - scrollX + line.offsets[(size_t)caret.col];
        float cy = textArea.y + caret.line * lineHeight - scrollY;
        DrawRectangle((int)cx, (int)cy + 2, 2, (int)lineHeight - 4, { 220, 220, 220, 255 });
    }
    EndScissorMode();

    BeginScissorMode((int)gutter.x, (int)gutter.y, (int)gutter.width, (int)gutter.height);
    for (int l = firstLine; l <= lastLine; l++) {
        float y = gutter.y + l * lineHeight - scrollY;
        std::string num = std::to_string(l + 1);
        float w = MeasureTextEx(font, num.c_str(), fontSize, spacing).x;
        bool current = l == caret.line;
        Color c = current ? Color{ 200, 200, 200, 255 } : Color{ 110, 110, 118, 255 };
        DrawTextEx(font, num.c_str(), { gutter.x + gutter.width - 12.0f - w, y + (lineHeight - fontSize) * 0.5f }, fontSize, spacing, c);
        for (const CodeDiagnostic& d : diagnostics) {
            if (d.line == l) { DrawCircle((int)gutter.x + 7, (int)(y + lineHeight * 0.5f), 3.0f, { 244, 71, 71, 255 }); break; }
        }
    }
    EndScissorMode();
    DrawLine((int)(gutter.x + gutter.width), (int)gutter.y, (int)(gutter.x + gutter.width), (int)(gutter.y + gutter.height), { 60, 60, 66, 255 });

    if (needV) {
        DrawRectangleRec(vBar, { 24, 24, 28, 255 });
        float maxY = std::max(1.0f, contentH - textArea.height);
        float thumbH = std::max(24.0f, vBar.height * std::min(1.0f, textArea.height / contentH));
        float thumbY = vBar.y + (vBar.height - thumbH) * (scrollY / maxY);
        DrawRectangle((int)vBar.x + 2, (int)thumbY, (int)vBar.width - 4, (int)thumbH, draggingVBar ? Color{ 130, 130, 140, 255 } : Color{ 90, 90, 98, 255 });
    }
    if (needH) {
        DrawRectangleRec(hBar, { 24, 24, 28, 255 });
        float contentW = contentWTmp;
        float maxX = std::max(1.0f, contentW - textArea.width);
        float thumbW = std::max(24.0f, hBar.width * std::min(1.0f, textArea.width / contentW));
        float thumbX = hBar.x + (hBar.width - thumbW) * (scrollX / maxX);
        DrawRectangle((int)thumbX, (int)hBar.y + 2, (int)thumbW, (int)hBar.height - 4, draggingHBar ? Color{ 130, 130, 140, 255 } : Color{ 90, 90, 98, 255 });
    }
    if (needV && needH) DrawRectangle((int)vBar.x, (int)hBar.y, (int)kBarSize, (int)kBarSize, { 24, 24, 28, 255 });

    DrawRectangleLinesEx(area, 1.0f, theme.panelBorder);

    if (completionOpen) DrawCompletionPopup(area);
    DrawDiagnosticTooltip(area);
}

void CodeEditor::DrawCompletionPopup(Rectangle area) {
    if (completionVisible.empty()) return;
    int rows = std::min(12, (int)completionVisible.size());
    float rowH = lineHeight + 2.0f;
    float width = 120.0f;
    for (size_t i = 0; i < completionVisible.size() && i < 60; i++) {
        const CompletionItem& it = completionAll[completionVisible[i]];
        float w = MeasureTextEx(font, it.label.c_str(), fontSize, spacing).x + 40.0f;
        if (!it.detail.empty()) w += MeasureTextEx(font, it.detail.c_str(), fontSize - 2.0f, spacing).x + 20.0f;
        width = std::max(width, w);
    }
    width = std::min(width, 420.0f);

    Line& line = lines[(size_t)caret.line];
    EnsureMetrics(line);
    int startCol = 0;
    WordBeforeCaret(startCol);
    float px = lastTextArea.x + kTextPad - scrollX + line.offsets[(size_t)startCol];
    float py = lastTextArea.y + (caret.line + 1) * lineHeight - scrollY;
    float height = rows * rowH + 4.0f;
    if (py + height > area.y + area.height) py = lastTextArea.y + caret.line * lineHeight - scrollY - height;
    if (px + width > area.x + area.width) px = area.x + area.width - width - 4.0f;
    px = std::max(px, area.x + 2.0f);
    Rectangle box = { px, py, width, height };

    WidgetInput::AddOccluder(box, WidgetInput::GetInputScope());
    DrawRectangleRec(box, { 37, 37, 42, 255 });
    DrawRectangleLinesEx(box, 1.0f, { 80, 80, 90, 255 });

    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, box)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) completionTop = std::max(0, std::min(completionTop - (int)wheel, (int)completionVisible.size() - rows));
    }

    for (int r = 0; r < rows; r++) {
        int idx = completionTop + r;
        if (idx >= (int)completionVisible.size()) break;
        const CompletionItem& it = completionAll[completionVisible[(size_t)idx]];
        Rectangle row = { box.x + 2, box.y + 2 + r * rowH, box.width - 4, rowH };
        bool hovered = CheckCollisionPointRec(mouse, row);
        if (idx == completionSelected) DrawRectangleRec(row, { 4, 57, 94, 255 });
        else if (hovered) DrawRectangleRec(row, { 255, 255, 255, 14 });
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { ApplyCompletion((size_t)idx); return; }

        Color badge;
        const char* letter = "k";
        switch (it.kind) {
            case CompletionKind::Keyword: badge = { 197, 134, 192, 255 }; letter = "k"; break;
            case CompletionKind::Type: badge = { 78, 201, 176, 255 }; letter = "T"; break;
            case CompletionKind::Function: badge = { 220, 220, 170, 255 }; letter = "f"; break;
            case CompletionKind::Variable: badge = { 156, 220, 254, 255 }; letter = "v"; break;
            case CompletionKind::Event: badge = { 255, 180, 100, 255 }; letter = "e"; break;
            case CompletionKind::Component: badge = { 78, 201, 176, 255 }; letter = "c"; break;
            case CompletionKind::Property: badge = { 156, 220, 254, 255 }; letter = "p"; break;
            case CompletionKind::Method: badge = { 220, 220, 170, 255 }; letter = "m"; break;
            default: badge = { 180, 180, 180, 255 }; letter = "s"; break;
        }
        DrawTextEx(font, letter, { row.x + 6, row.y + (rowH - fontSize) * 0.5f }, fontSize, spacing, badge);
        DrawTextEx(font, it.label.c_str(), { row.x + 24, row.y + (rowH - fontSize) * 0.5f }, fontSize, spacing, { 220, 220, 225, 255 });
        if (!it.detail.empty()) {
            float lw = MeasureTextEx(font, it.label.c_str(), fontSize, spacing).x;
            DrawTextEx(font, it.detail.c_str(), { row.x + 24 + lw + 12, row.y + (rowH - fontSize + 2.0f) * 0.5f }, fontSize - 2.0f, spacing, { 130, 130, 140, 255 });
        }
    }
}

void CodeEditor::DrawDiagnosticTooltip(Rectangle area) {
    if (diagnostics.empty() || completionOpen) return;
    Vector2 mouse = GetMousePosition();
    const void* scope = WidgetInput::GetInputScope();
    if (!CheckCollisionPointRec(mouse, lastTextArea) || WidgetInput::IsMouseBlocked() || (scope && WidgetInput::IsMouseOccluded(mouse, scope))) return;

    int hoverLine = (int)std::floor((mouse.y - lastTextArea.y + scrollY) / lineHeight);
    if (hoverLine < 0 || hoverLine >= (int)lines.size()) return;
    const CodeDiagnostic* hit = nullptr;
    Line& line = lines[(size_t)hoverLine];
    EnsureMetrics(line);
    float lineEndX = lastTextArea.x + kTextPad - scrollX + std::max(line.width, 24.0f);
    if (mouse.x > lineEndX + 40.0f) return;
    for (const CodeDiagnostic& d : diagnostics) {
        if (d.line == hoverLine) { hit = &d; break; }
    }
    if (!hit) return;

    std::string text = hit->message;
    float w = MeasureTextEx(font, text.c_str(), fontSize - 1.0f, spacing).x + 16.0f;
    float h = lineHeight + 4.0f;
    float x = std::min(mouse.x + 12.0f, area.x + area.width - w - 4.0f);
    float y = mouse.y + 18.0f;
    if (y + h > area.y + area.height) y = mouse.y - h - 6.0f;
    DrawRectangle((int)x, (int)y, (int)w, (int)h, { 50, 28, 30, 245 });
    DrawRectangleLinesEx({ x, y, w, h }, 1.0f, { 244, 71, 71, 255 });
    DrawTextEx(font, text.c_str(), { x + 8.0f, y + (h - fontSize + 1.0f) * 0.5f }, fontSize - 1.0f, spacing, { 255, 200, 200, 255 });
}
