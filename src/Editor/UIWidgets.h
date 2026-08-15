#pragma once
#include "raylib.h"
#include <string>

namespace UIWidget {

struct Interaction {
    bool hovered = false;
    bool pressed = false;
    bool clicked = false;
};

Interaction Interact(Rectangle r, bool enabled = true);

bool Button(Rectangle r, const char* label, bool enabled = true);
bool IconButton(Rectangle r, const char* label, bool enabled = true);

void Panel(Rectangle r);
Color SelectableBg(bool hovered, bool selected, Color idle);

void ScrollableTextBox(Rectangle r, const std::string& content, float& scrollOffset);

}