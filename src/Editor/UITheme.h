#pragma once
#include "raylib.h"
#include <string>

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

struct UIColorFieldInfo {
    const char* name;
    Color UIPalette::* member;
};

namespace UITheme {

const UIPalette& Get();
Font GetFont();

int PresetCount();
const char* PresetName(int index);
int GetPresetIndex();
void SetPreset(int index);

int FieldCount();
const UIColorFieldInfo& Field(int index);

void SetOverride(int fieldIndex, Color value);
void ClearOverride(int fieldIndex);
void ClearAllOverrides();
bool HasOverride(int fieldIndex);

void Load();
void Save();

bool ExportToFile(const std::string& path);
bool ImportFromFile(const std::string& path);

}