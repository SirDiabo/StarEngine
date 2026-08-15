#include "UIWidgets.h"
#include "UITheme.h"
#include <vector>

namespace UIWidget {

Interaction Interact(Rectangle r, bool enabled) {
    Interaction s;
    if (!enabled) return s;
    Vector2 mp = GetMousePosition();
    s.hovered = CheckCollisionPointRec(mp, r);
    s.pressed = s.hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    s.clicked = s.hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    return s;
}

bool Button(Rectangle r, const char* label, bool enabled) {
    Interaction s = Interact(r, enabled);
    const UIPalette& theme = UITheme::Get();

    Color bg = theme.buttonBg;
    if (enabled && s.pressed) bg = theme.buttonPressed;
    else if (enabled && s.hovered) bg = theme.buttonHover;

    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 1, theme.buttonBorder);

    Color textColor = enabled ? theme.buttonText : theme.buttonDisabledText;
    int textWidth = MeasureText(label, 11);
    DrawText(label, (int)(r.x + (r.width - textWidth) / 2.0f), (int)(r.y + (r.height - 11) / 2.0f), 11, textColor);

    return s.clicked;
}

bool IconButton(Rectangle r, const char* label, bool enabled) {
    Interaction s = Interact(r, enabled);
    const UIPalette& theme = UITheme::Get();

    if (enabled && s.hovered) DrawRectangleRec(r, theme.danger);

    Color textColor = (enabled && s.hovered) ? theme.buttonText : theme.textSecondary;
    int textWidth = MeasureText(label, 10);
    DrawText(label, (int)(r.x + (r.width - textWidth) / 2.0f), (int)(r.y + (r.height - 10) / 2.0f), 10, textColor);

    return s.clicked;
}

void Panel(Rectangle r) {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(r, theme.panelBg);
    DrawRectangleLinesEx(r, 1, theme.panelBorder);
}

Color SelectableBg(bool hovered, bool selected, Color idle) {
    const UIPalette& theme = UITheme::Get();
    if (selected) return theme.surfaceSelected;
    return hovered ? theme.surfaceHover : idle;
}

static void WrapLineInto(const std::string& line, int fontSize, float maxWidth, std::vector<std::string>& out) {
    if (line.empty()) {
        out.push_back("");
        return;
    }

    std::string current;
    for (char c : line) {
        std::string candidate = current + c;
        if (!current.empty() && MeasureText(candidate.c_str(), fontSize) > maxWidth) {
            out.push_back(current);
            current = std::string(1, c);
        } else {
            current = candidate;
        }
    }
    out.push_back(current);
}

void ScrollableTextBox(Rectangle r, const std::string& content, float& scrollOffset) {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(r, theme.fieldBg);
    DrawRectangleLinesEx(r, 1, theme.fieldBorder);

    float lineHeight = 13.0f;
    float padding = 6.0f;
    int fontSize = 10;
    float maxTextWidth = r.width - padding * 2 - 8.0f;

    std::vector<std::string> lines;
    std::string rawLine;
    for (char c : content) {
        if (c == '\n') {
            WrapLineInto(rawLine, fontSize, maxTextWidth, lines);
            rawLine.clear();
        } else {
            rawLine += c;
        }
    }
    WrapLineInto(rawLine, fontSize, maxTextWidth, lines);

    float contentHeight = (float)lines.size() * lineHeight;
    float visibleHeight = r.height - padding * 2;

    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, r)) {
        scrollOffset -= GetMouseWheelMove() * lineHeight * 3;
    }

    float maxScroll = (contentHeight > visibleHeight) ? (contentHeight - visibleHeight) : 0.0f;
    if (scrollOffset < 0.0f) scrollOffset = 0.0f;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;

    BeginScissorMode((int)r.x + 1, (int)r.y + 1, (int)r.width - 2, (int)r.height - 2);
    float y = r.y + padding - scrollOffset;
    for (const std::string& l : lines) {
        if (y > r.y - lineHeight && y < r.y + r.height) {
            DrawText(l.c_str(), (int)(r.x + padding), (int)y, fontSize, theme.textSecondary);
        }
        y += lineHeight;
    }
    EndScissorMode();

    if (contentHeight > visibleHeight) {
        float scrollRatio = (maxScroll > 0.0f) ? scrollOffset / maxScroll : 0.0f;
        float barHeight = (visibleHeight / contentHeight) * visibleHeight;
        if (barHeight < 20.0f) barHeight = 20.0f;
        float barY = r.y + padding + scrollRatio * (visibleHeight - barHeight);
        DrawRectangleRec({r.x + r.width - 7, barY, 4, barHeight}, {100, 100, 100, 180});
        DrawRectangleLinesEx({r.x + r.width - 7, barY, 4, barHeight}, 1, {150, 150, 150, 255});
    }
}

}