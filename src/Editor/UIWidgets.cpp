#include "UIWidgets.h"
#include "UITheme.h"

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

Color SelectableBg(bool hovered, bool selected) {
    const UIPalette& theme = UITheme::Get();
    if (selected) return theme.surfaceSelected;
    return hovered ? theme.surfaceHover : theme.panelBg;
}

}
