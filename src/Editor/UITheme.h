#pragma once
#include "raylib.h"

enum class ThemeMode {
    Dark,
    Light,
};

struct UIPalette {
    Color windowBg;
    Color panelBg;
    Color panelBorder;

    Color tabBar;
    Color tabText;

    Color surfaceHover;
    Color surfaceSelected;

    Color splitter;
    Color splitterHover;

    Color dropZone;
    Color dropBorder;

    Color buttonBg;
    Color buttonHover;
    Color buttonPressed;
    Color buttonBorder;
    Color buttonText;
    Color buttonDisabledText;

    Color fieldBg;
    Color fieldBorder;

    Color textPrimary;
    Color textSecondary;

    Color accent;
    Color danger;
    Color dangerBg;
    Color success;
};

namespace UITheme {

const UIPalette& Get();
ThemeMode GetMode();
void SetMode(ThemeMode mode);

}