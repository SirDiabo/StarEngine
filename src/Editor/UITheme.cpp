#include "UITheme.h"

static UIPalette MakeDarkPalette() {
    UIPalette p;
    p.windowBg = {24, 24, 27, 255};
    p.panelBg = {38, 38, 42, 255};
    p.panelBorder = {70, 70, 80, 255};

    p.tabBar = {35, 35, 38, 255};
    p.tabText = {200, 200, 200, 255};

    p.surfaceHover = {45, 45, 50, 255};
    p.surfaceSelected = {60, 80, 120, 255};

    p.splitter = {28, 28, 30, 255};
    p.splitterHover = {80, 120, 200, 255};

    p.dropZone = {60, 120, 220, 80};
    p.dropBorder = {80, 150, 255, 200};

    p.buttonBg = {48, 48, 54, 255};
    p.buttonHover = {60, 60, 68, 255};
    p.buttonPressed = {70, 100, 150, 255};
    p.buttonBorder = {80, 80, 90, 255};
    p.buttonText = {220, 220, 220, 255};
    p.buttonDisabledText = {110, 110, 115, 255};

    p.fieldBg = {38, 38, 42, 255};
    p.fieldBorder = {70, 70, 80, 255};

    p.textPrimary = {220, 220, 220, 255};
    p.textSecondary = {150, 150, 160, 255};
    p.sectionHeader = {180, 200, 230, 255};

    p.accent = {100, 160, 230, 255};
    p.danger = {190, 70, 70, 255};
    p.dangerBg = {160, 55, 55, 255};
    p.success = {40, 160, 75, 255};
    p.successBg = {30, 130, 60, 255};
    return p;
}

static UIPalette MakeLightPalette() {
    UIPalette p;
    p.windowBg = {245, 245, 247, 255};
    p.panelBg = {255, 255, 255, 255};
    p.panelBorder = {210, 210, 215, 255};

    p.tabBar = {230, 230, 233, 255};
    p.tabText = {40, 40, 44, 255};

    p.surfaceHover = {225, 225, 230, 255};
    p.surfaceSelected = {190, 215, 245, 255};

    p.splitter = {215, 215, 219, 255};
    p.splitterHover = {80, 120, 200, 255};

    p.dropZone = {60, 120, 220, 60};
    p.dropBorder = {60, 120, 220, 200};

    p.buttonBg = {240, 240, 242, 255};
    p.buttonHover = {225, 225, 230, 255};
    p.buttonPressed = {190, 215, 245, 255};
    p.buttonBorder = {200, 200, 205, 255};
    p.buttonText = {35, 35, 38, 255};
    p.buttonDisabledText = {170, 170, 175, 255};

    p.fieldBg = {252, 252, 253, 255};
    p.fieldBorder = {205, 205, 210, 255};

    p.textPrimary = {30, 30, 34, 255};
    p.textSecondary = {110, 110, 120, 255};
    p.sectionHeader = {40, 90, 165, 255};

    p.accent = {50, 110, 200, 255};
    p.danger = {195, 70, 70, 255};
    p.dangerBg = {175, 60, 60, 255};
    p.success = {50, 140, 80, 255};
    p.successBg = {40, 120, 65, 255};
    return p;
}

static ThemeMode g_mode = ThemeMode::Dark;
static UIPalette g_palette = MakeDarkPalette();

namespace UITheme {

const UIPalette& Get() {
    return g_palette;
}

ThemeMode GetMode() {
    return g_mode;
}

void SetMode(ThemeMode mode) {
    g_mode = mode;
    g_palette = mode == ThemeMode::Light ? MakeLightPalette() : MakeDarkPalette();
}

}