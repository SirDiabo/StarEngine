#pragma once
#include "raylib.h"

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
    Color sectionHeader;

    Color accent;
    Color danger;
    Color dangerBg;
    Color success;
    Color successBg;
};

namespace UITheme {

const UIPalette& Get();

int PresetCount();
const char* PresetName(int index);
int GetPresetIndex();
void SetPreset(int index);

void Load();
void Save();

}