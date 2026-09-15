#include "UITheme.h"
#include "Core/LayoutSerializer.h"
#include "Core/Widgets/WidgetFont.h"
#include <filesystem>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>

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

static UIPalette MakeMidnightPalette() {
    UIPalette p;
    p.windowBg = {14, 16, 26, 255};
    p.panelBg = {24, 27, 40, 255};
    p.panelBorder = {50, 55, 78, 255};

    p.tabBar = {20, 23, 35, 255};
    p.tabText = {195, 200, 220, 255};

    p.surfaceHover = {34, 38, 55, 255};
    p.surfaceSelected = {50, 55, 100, 255};

    p.splitter = {16, 18, 28, 255};
    p.splitterHover = {90, 100, 200, 255};

    p.dropZone = {70, 90, 220, 80};
    p.dropBorder = {100, 120, 240, 200};

    p.buttonBg = {32, 35, 52, 255};
    p.buttonHover = {42, 46, 66, 255};
    p.buttonPressed = {70, 75, 130, 255};
    p.buttonBorder = {60, 65, 90, 255};
    p.buttonText = {220, 222, 235, 255};
    p.buttonDisabledText = {100, 103, 125, 255};

    p.fieldBg = {22, 25, 38, 255};
    p.fieldBorder = {50, 55, 78, 255};

    p.textPrimary = {220, 222, 235, 255};
    p.textSecondary = {140, 145, 175, 255};
    p.sectionHeader = {160, 175, 230, 255};

    p.accent = {110, 130, 240, 255};
    p.danger = {200, 75, 90, 255};
    p.dangerBg = {165, 58, 72, 255};
    p.success = {60, 170, 130, 255};
    p.successBg = {45, 135, 102, 255};
    return p;
}

static UIPalette MakeSlatePalette() {
    UIPalette p;
    p.windowBg = {22, 24, 27, 255};
    p.panelBg = {34, 37, 41, 255};
    p.panelBorder = {60, 65, 71, 255};

    p.tabBar = {30, 33, 37, 255};
    p.tabText = {195, 200, 205, 255};

    p.surfaceHover = {42, 46, 51, 255};
    p.surfaceSelected = {45, 75, 85, 255};

    p.splitter = {25, 27, 30, 255};
    p.splitterHover = {70, 150, 165, 255};

    p.dropZone = {60, 160, 180, 80};
    p.dropBorder = {80, 190, 210, 200};

    p.buttonBg = {42, 46, 51, 255};
    p.buttonHover = {54, 59, 65, 255};
    p.buttonPressed = {55, 100, 110, 255};
    p.buttonBorder = {72, 78, 85, 255};
    p.buttonText = {215, 218, 220, 255};
    p.buttonDisabledText = {105, 108, 112, 255};

    p.fieldBg = {34, 37, 41, 255};
    p.fieldBorder = {60, 65, 71, 255};

    p.textPrimary = {215, 218, 220, 255};
    p.textSecondary = {145, 150, 155, 255};
    p.sectionHeader = {150, 200, 210, 255};

    p.accent = {70, 180, 195, 255};
    p.danger = {195, 75, 75, 255};
    p.dangerBg = {162, 58, 58, 255};
    p.success = {60, 170, 120, 255};
    p.successBg = {45, 135, 95, 255};
    return p;
}

static UIPalette MakeForestPalette() {
    UIPalette p;
    p.windowBg = {18, 22, 18, 255};
    p.panelBg = {28, 34, 28, 255};
    p.panelBorder = {55, 65, 52, 255};

    p.tabBar = {24, 29, 24, 255};
    p.tabText = {200, 205, 195, 255};

    p.surfaceHover = {36, 44, 36, 255};
    p.surfaceSelected = {55, 80, 50, 255};

    p.splitter = {20, 24, 20, 255};
    p.splitterHover = {110, 170, 90, 255};

    p.dropZone = {90, 170, 80, 80};
    p.dropBorder = {120, 200, 100, 200};

    p.buttonBg = {38, 45, 37, 255};
    p.buttonHover = {48, 57, 46, 255};
    p.buttonPressed = {70, 110, 62, 255};
    p.buttonBorder = {65, 75, 60, 255};
    p.buttonText = {220, 224, 215, 255};
    p.buttonDisabledText = {105, 110, 100, 255};

    p.fieldBg = {28, 34, 28, 255};
    p.fieldBorder = {55, 65, 52, 255};

    p.textPrimary = {220, 224, 215, 255};
    p.textSecondary = {150, 158, 145, 255};
    p.sectionHeader = {170, 210, 140, 255};

    p.accent = {120, 190, 95, 255};
    p.danger = {200, 90, 70, 255};
    p.dangerBg = {165, 68, 52, 255};
    p.success = {90, 180, 90, 255};
    p.successBg = {65, 145, 68, 255};
    return p;
}

static UIPalette MakeSandPalette() {
    UIPalette p;
    p.windowBg = {245, 238, 225, 255};
    p.panelBg = {250, 245, 235, 255};
    p.panelBorder = {220, 205, 185, 255};

    p.tabBar = {235, 225, 208, 255};
    p.tabText = {60, 50, 38, 255};

    p.surfaceHover = {232, 220, 200, 255};
    p.surfaceSelected = {235, 195, 160, 255};

    p.splitter = {222, 208, 188, 255};
    p.splitterHover = {210, 130, 80, 255};

    p.dropZone = {210, 130, 80, 60};
    p.dropBorder = {200, 110, 60, 200};

    p.buttonBg = {242, 234, 220, 255};
    p.buttonHover = {230, 218, 198, 255};
    p.buttonPressed = {225, 180, 145, 255};
    p.buttonBorder = {205, 190, 168, 255};
    p.buttonText = {55, 45, 34, 255};
    p.buttonDisabledText = {160, 148, 130, 255};

    p.fieldBg = {252, 248, 240, 255};
    p.fieldBorder = {212, 197, 175, 255};

    p.textPrimary = {50, 42, 32, 255};
    p.textSecondary = {120, 105, 88, 255};
    p.sectionHeader = {175, 95, 45, 255};

    p.accent = {195, 105, 50, 255};
    p.danger = {190, 75, 60, 255};
    p.dangerBg = {172, 65, 52, 255};
    p.success = {90, 135, 65, 255};
    p.successBg = {75, 118, 55, 255};
    return p;
}

static UIPalette MakeMatchaLattePalette() {
    UIPalette p;
    p.windowBg = {244, 246, 232, 255};
    p.panelBg = {250, 251, 240, 255};
    p.panelBorder = {205, 212, 175, 255};

    p.tabBar = {228, 233, 200, 255};
    p.tabText = {55, 65, 35, 255};

    p.surfaceHover = {225, 232, 195, 255};
    p.surfaceSelected = {195, 215, 140, 255};

    p.splitter = {210, 218, 180, 255};
    p.splitterHover = {130, 175, 70, 255};

    p.dropZone = {130, 175, 70, 60};
    p.dropBorder = {110, 160, 55, 200};

    p.buttonBg = {238, 242, 215, 255};
    p.buttonHover = {222, 230, 185, 255};
    p.buttonPressed = {190, 212, 130, 255};
    p.buttonBorder = {195, 205, 160, 255};
    p.buttonText = {50, 60, 30, 255};
    p.buttonDisabledText = {150, 158, 120, 255};

    p.fieldBg = {250, 251, 240, 255};
    p.fieldBorder = {200, 208, 168, 255};

    p.textPrimary = {45, 55, 28, 255};
    p.textSecondary = {110, 120, 78, 255};
    p.sectionHeader = {95, 135, 45, 255};

    p.accent = {120, 165, 55, 255};
    p.danger = {195, 75, 65, 255};
    p.dangerBg = {178, 65, 55, 255};
    p.success = {85, 150, 60, 255};
    p.successBg = {68, 130, 48, 255};
    return p;
}

static UIPalette MakeSunsetPalette() {
    UIPalette p;
    p.windowBg = {22, 20, 19, 255};
    p.panelBg = {34, 31, 29, 255};
    p.panelBorder = {65, 58, 52, 255};

    p.tabBar = {30, 27, 25, 255};
    p.tabText = {215, 205, 195, 255};

    p.surfaceHover = {44, 40, 37, 255};
    p.surfaceSelected = {95, 65, 40, 255};

    p.splitter = {26, 23, 21, 255};
    p.splitterHover = {230, 140, 60, 255};

    p.dropZone = {230, 140, 60, 80};
    p.dropBorder = {245, 165, 90, 200};

    p.buttonBg = {44, 40, 37, 255};
    p.buttonHover = {56, 50, 45, 255};
    p.buttonPressed = {150, 95, 50, 255};
    p.buttonBorder = {75, 66, 58, 255};
    p.buttonText = {225, 216, 208, 255};
    p.buttonDisabledText = {110, 102, 95, 255};

    p.fieldBg = {34, 31, 29, 255};
    p.fieldBorder = {65, 58, 52, 255};

    p.textPrimary = {225, 216, 208, 255};
    p.textSecondary = {155, 143, 132, 255};
    p.sectionHeader = {235, 155, 90, 255};

    p.accent = {230, 140, 60, 255};
    p.danger = {205, 75, 65, 255};
    p.dangerBg = {170, 62, 55, 255};
    p.success = {130, 175, 80, 255};
    p.successBg = {100, 145, 62, 255};
    return p;
}

static UIPalette MakeHoneyPalette() {
    UIPalette p;
    p.windowBg = {250, 244, 225, 255};
    p.panelBg = {255, 250, 235, 255};
    p.panelBorder = {225, 205, 155, 255};

    p.tabBar = {240, 225, 180, 255};
    p.tabText = {70, 55, 20, 255};

    p.surfaceHover = {235, 218, 165, 255};
    p.surfaceSelected = {240, 195, 100, 255};

    p.splitter = {228, 210, 165, 255};
    p.splitterHover = {220, 150, 40, 255};

    p.dropZone = {220, 150, 40, 60};
    p.dropBorder = {200, 130, 20, 200};

    p.buttonBg = {246, 236, 205, 255};
    p.buttonHover = {235, 218, 175, 255};
    p.buttonPressed = {230, 175, 90, 255};
    p.buttonBorder = {210, 190, 140, 255};
    p.buttonText = {60, 48, 18, 255};
    p.buttonDisabledText = {165, 150, 110, 255};

    p.fieldBg = {253, 248, 232, 255};
    p.fieldBorder = {215, 197, 148, 255};

    p.textPrimary = {55, 43, 15, 255};
    p.textSecondary = {130, 112, 68, 255};
    p.sectionHeader = {180, 120, 20, 255};

    p.accent = {225, 155, 30, 255};
    p.danger = {195, 75, 65, 255};
    p.dangerBg = {178, 65, 55, 255};
    p.success = {95, 140, 60, 255};
    p.successBg = {78, 122, 50, 255};
    return p;
}

static UIPalette MakeLavenderPalette() {
    UIPalette p;
    p.windowBg = {242, 238, 248, 255};
    p.panelBg = {249, 246, 253, 255};
    p.panelBorder = {205, 195, 225, 255};

    p.tabBar = {225, 215, 240, 255};
    p.tabText = {55, 45, 75, 255};

    p.surfaceHover = {220, 208, 238, 255};
    p.surfaceSelected = {195, 170, 225, 255};

    p.splitter = {212, 202, 232, 255};
    p.splitterHover = {150, 100, 210, 255};

    p.dropZone = {150, 100, 210, 60};
    p.dropBorder = {130, 80, 195, 200};

    p.buttonBg = {238, 232, 248, 255};
    p.buttonHover = {222, 212, 240, 255};
    p.buttonPressed = {190, 160, 220, 255};
    p.buttonBorder = {200, 188, 222, 255};
    p.buttonText = {50, 42, 70, 255};
    p.buttonDisabledText = {150, 142, 168, 255};

    p.fieldBg = {250, 247, 253, 255};
    p.fieldBorder = {202, 192, 224, 255};

    p.textPrimary = {45, 38, 65, 255};
    p.textSecondary = {112, 100, 138, 255};
    p.sectionHeader = {130, 75, 195, 255};

    p.accent = {150, 100, 215, 255};
    p.danger = {195, 75, 75, 255};
    p.dangerBg = {178, 65, 65, 255};
    p.success = {80, 150, 100, 255};
    p.successBg = {65, 130, 82, 255};
    return p;
}

static UIPalette MakeDaisyPalette() {
    UIPalette p;
    p.windowBg = {250, 250, 240, 255};
    p.panelBg = {255, 255, 250, 255};
    p.panelBorder = {225, 220, 170, 255};

    p.tabBar = {245, 240, 195, 255};
    p.tabText = {60, 58, 30, 255};

    p.surfaceHover = {240, 235, 190, 255};
    p.surfaceSelected = {250, 225, 90, 255};

    p.splitter = {230, 226, 185, 255};
    p.splitterHover = {235, 195, 30, 255};

    p.dropZone = {235, 195, 30, 60};
    p.dropBorder = {210, 170, 15, 200};

    p.buttonBg = {252, 250, 230, 255};
    p.buttonHover = {245, 240, 200, 255};
    p.buttonPressed = {245, 215, 80, 255};
    p.buttonBorder = {218, 212, 165, 255};
    p.buttonText = {55, 52, 25, 255};
    p.buttonDisabledText = {165, 160, 120, 255};

    p.fieldBg = {255, 255, 245, 255};
    p.fieldBorder = {222, 216, 165, 255};

    p.textPrimary = {50, 48, 22, 255};
    p.textSecondary = {125, 118, 65, 255};
    p.sectionHeader = {170, 140, 10, 255};

    p.accent = {235, 195, 30, 255};
    p.danger = {195, 75, 65, 255};
    p.dangerBg = {178, 65, 55, 255};
    p.success = {90, 150, 65, 255};
    p.successBg = {72, 130, 50, 255};
    return p;
}

static UIPalette MakeMintPalette() {
    UIPalette p;
    p.windowBg = {238, 248, 240, 255};
    p.panelBg = {246, 253, 248, 255};
    p.panelBorder = {180, 220, 190, 255};

    p.tabBar = {210, 238, 215, 255};
    p.tabText = {30, 65, 42, 255};

    p.surfaceHover = {200, 232, 205, 255};
    p.surfaceSelected = {150, 220, 165, 255};

    p.splitter = {195, 228, 200, 255};
    p.splitterHover = {60, 195, 120, 255};

    p.dropZone = {60, 195, 120, 60};
    p.dropBorder = {40, 175, 100, 200};

    p.buttonBg = {232, 248, 235, 255};
    p.buttonHover = {208, 235, 212, 255};
    p.buttonPressed = {130, 210, 155, 255};
    p.buttonBorder = {185, 220, 190, 255};
    p.buttonText = {25, 58, 38, 255};
    p.buttonDisabledText = {140, 165, 148, 255};

    p.fieldBg = {248, 253, 249, 255};
    p.fieldBorder = {188, 222, 195, 255};

    p.textPrimary = {22, 55, 35, 255};
    p.textSecondary = {90, 128, 100, 255};
    p.sectionHeader = {30, 145, 85, 255};

    p.accent = {50, 180, 110, 255};
    p.danger = {195, 75, 65, 255};
    p.dangerBg = {178, 65, 55, 255};
    p.success = {45, 165, 95, 255};
    p.successBg = {35, 145, 80, 255};
    return p;
}

static UIPalette MakeCottonCandyPalette() {
    UIPalette p;
    p.windowBg = {252, 240, 245, 255};
    p.panelBg = {255, 248, 250, 255};
    p.panelBorder = {235, 195, 210, 255};

    p.tabBar = {248, 220, 232, 255};
    p.tabText = {75, 35, 50, 255};

    p.surfaceHover = {245, 210, 225, 255};
    p.surfaceSelected = {250, 165, 195, 255};

    p.splitter = {238, 212, 224, 255};
    p.splitterHover = {235, 100, 155, 255};

    p.dropZone = {235, 100, 155, 60};
    p.dropBorder = {220, 75, 135, 200};

    p.buttonBg = {252, 232, 240, 255};
    p.buttonHover = {247, 215, 228, 255};
    p.buttonPressed = {245, 155, 185, 255};
    p.buttonBorder = {230, 200, 213, 255};
    p.buttonText = {70, 32, 45, 255};
    p.buttonDisabledText = {175, 145, 155, 255};

    p.fieldBg = {254, 245, 248, 255};
    p.fieldBorder = {233, 200, 213, 255};

    p.textPrimary = {65, 28, 40, 255};
    p.textSecondary = {140, 100, 115, 255};
    p.sectionHeader = {200, 55, 115, 255};

    p.accent = {235, 105, 160, 255};
    p.danger = {200, 70, 70, 255};
    p.dangerBg = {182, 60, 60, 255};
    p.success = {85, 150, 95, 255};
    p.successBg = {68, 130, 78, 255};
    return p;
}

static UIPalette MakeBlueberryPalette() {
    UIPalette p;
    p.windowBg = {16, 18, 32, 255};
    p.panelBg = {26, 29, 48, 255};
    p.panelBorder = {52, 56, 88, 255};

    p.tabBar = {21, 24, 40, 255};
    p.tabText = {200, 202, 225, 255};

    p.surfaceHover = {36, 40, 62, 255};
    p.surfaceSelected = {55, 62, 115, 255};

    p.splitter = {18, 20, 34, 255};
    p.splitterHover = {100, 110, 220, 255};

    p.dropZone = {100, 110, 220, 80};
    p.dropBorder = {130, 140, 240, 200};

    p.buttonBg = {33, 36, 58, 255};
    p.buttonHover = {44, 48, 74, 255};
    p.buttonPressed = {75, 82, 145, 255};
    p.buttonBorder = {60, 64, 98, 255};
    p.buttonText = {220, 222, 238, 255};
    p.buttonDisabledText = {100, 104, 130, 255};

    p.fieldBg = {24, 27, 45, 255};
    p.fieldBorder = {52, 56, 88, 255};

    p.textPrimary = {222, 224, 240, 255};
    p.textSecondary = {140, 145, 180, 255};
    p.sectionHeader = {150, 165, 235, 255};

    p.accent = {110, 125, 235, 255};
    p.danger = {205, 80, 95, 255};
    p.dangerBg = {170, 62, 75, 255};
    p.success = {70, 175, 135, 255};
    p.successBg = {52, 140, 105, 255};
    return p;
}

static UIPalette MakePlumPalette() {
    UIPalette p;
    p.windowBg = {24, 16, 24, 255};
    p.panelBg = {38, 27, 38, 255};
    p.panelBorder = {70, 52, 70, 255};

    p.tabBar = {32, 22, 32, 255};
    p.tabText = {215, 200, 215, 255};

    p.surfaceHover = {50, 37, 50, 255};
    p.surfaceSelected = {95, 55, 95, 255};

    p.splitter = {26, 18, 26, 255};
    p.splitterHover = {200, 90, 190, 255};

    p.dropZone = {200, 90, 190, 80};
    p.dropBorder = {225, 115, 215, 200};

    p.buttonBg = {46, 33, 46, 255};
    p.buttonHover = {58, 43, 58, 255};
    p.buttonPressed = {120, 65, 118, 255};
    p.buttonBorder = {78, 60, 78, 255};
    p.buttonText = {228, 216, 228, 255};
    p.buttonDisabledText = {115, 100, 115, 255};

    p.fieldBg = {36, 26, 36, 255};
    p.fieldBorder = {70, 52, 70, 255};

    p.textPrimary = {228, 216, 228, 255};
    p.textSecondary = {162, 142, 162, 255};
    p.sectionHeader = {225, 140, 215, 255};

    p.accent = {195, 90, 190, 255};
    p.danger = {210, 78, 88, 255};
    p.dangerBg = {172, 62, 70, 255};
    p.success = {90, 170, 120, 255};
    p.successBg = {68, 138, 95, 255};
    return p;
}

static UIPalette MakeCocoaPalette() {
    UIPalette p;
    p.windowBg = {24, 19, 16, 255};
    p.panelBg = {37, 30, 25, 255};
    p.panelBorder = {68, 55, 45, 255};

    p.tabBar = {31, 25, 21, 255};
    p.tabText = {215, 200, 188, 255};

    p.surfaceHover = {48, 39, 32, 255};
    p.surfaceSelected = {100, 70, 45, 255};

    p.splitter = {26, 21, 17, 255};
    p.splitterHover = {215, 140, 60, 255};

    p.dropZone = {215, 140, 60, 80};
    p.dropBorder = {235, 165, 90, 200};

    p.buttonBg = {45, 36, 30, 255};
    p.buttonHover = {57, 46, 38, 255};
    p.buttonPressed = {150, 100, 55, 255};
    p.buttonBorder = {76, 62, 51, 255};
    p.buttonText = {224, 210, 198, 255};
    p.buttonDisabledText = {112, 98, 88, 255};

    p.fieldBg = {35, 28, 23, 255};
    p.fieldBorder = {68, 55, 45, 255};

    p.textPrimary = {224, 210, 198, 255};
    p.textSecondary = {158, 140, 122, 255};
    p.sectionHeader = {225, 150, 80, 255};

    p.accent = {210, 135, 60, 255};
    p.danger = {205, 80, 68, 255};
    p.dangerBg = {170, 65, 55, 255};
    p.success = {120, 165, 80, 255};
    p.successBg = {95, 138, 62, 255};
    return p;
}

struct ThemePreset {
    const char* name;
    UIPalette (*make)();
};

static const ThemePreset kPresets[] = {
    {"Dark", MakeDarkPalette},
    {"Light", MakeLightPalette},
    {"Midnight", MakeMidnightPalette},
    {"Slate", MakeSlatePalette},
    {"Forest", MakeForestPalette},
    {"Sand", MakeSandPalette},
    {"Matcha Latte", MakeMatchaLattePalette},
    {"Sunset", MakeSunsetPalette},
    {"Honey", MakeHoneyPalette},
    {"Lavender", MakeLavenderPalette},
    {"Daisy", MakeDaisyPalette},
    {"Mint", MakeMintPalette},
    {"Cotton Candy", MakeCottonCandyPalette},
    {"Blueberry", MakeBlueberryPalette},
    {"Plum", MakePlumPalette},
    {"Cocoa", MakeCocoaPalette},
};
static const int kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

static const UIColorFieldInfo kFields[] = {
    {"Window Background",      &UIPalette::windowBg},
    {"Panel Background",       &UIPalette::panelBg},
    {"Panel Border",           &UIPalette::panelBorder},
    {"Tab Bar",                &UIPalette::tabBar},
    {"Tab Text",               &UIPalette::tabText},
    {"Hover Surface",          &UIPalette::surfaceHover},
    {"Selected Surface",       &UIPalette::surfaceSelected},
    {"Splitter",               &UIPalette::splitter},
    {"Splitter (Hover)",       &UIPalette::splitterHover},
    {"Drop Zone",              &UIPalette::dropZone},
    {"Drop Zone Border",       &UIPalette::dropBorder},
    {"Button",                 &UIPalette::buttonBg},
    {"Button (Hover)",         &UIPalette::buttonHover},
    {"Button (Pressed)",       &UIPalette::buttonPressed},
    {"Button Border",          &UIPalette::buttonBorder},
    {"Button Text",            &UIPalette::buttonText},
    {"Button Text (Disabled)", &UIPalette::buttonDisabledText},
    {"Field Background",       &UIPalette::fieldBg},
    {"Field Border",           &UIPalette::fieldBorder},
    {"Primary Text",           &UIPalette::textPrimary},
    {"Secondary Text",         &UIPalette::textSecondary},
    {"Section Header",         &UIPalette::sectionHeader},
    {"Accent",                 &UIPalette::accent},
    {"Danger (Hover)",         &UIPalette::danger},
    {"Danger",                 &UIPalette::dangerBg},
    {"Success (Hover)",        &UIPalette::success},
    {"Success",                &UIPalette::successBg},
};
static const int kFieldCount = sizeof(kFields) / sizeof(kFields[0]);

static int g_presetIndex = 0;
static UIPalette g_basePalette = MakeDarkPalette();
static UIPalette g_effectivePalette = MakeDarkPalette();

static bool g_hasOverride[kFieldCount] = {};
static Color g_overrideValue[kFieldCount] = {};

static void RecomputeEffectivePalette() {
    g_effectivePalette = g_basePalette;
    for (int i = 0; i < kFieldCount; i++) {
        if (g_hasOverride[i]) g_effectivePalette.*(kFields[i].member) = g_overrideValue[i];
    }
}

static std::string GetThemeConfigPath() {
    std::string home;
#ifdef _WIN32
    char* buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, "USERPROFILE") == 0 && buf) {
        home = buf;
        free(buf);
    } else {
        home = ".";
    }
#else
    const char* h = getenv("HOME");
    home = h ? h : ".";
#endif
    std::filesystem::path dir = std::filesystem::path(home) / ".starengine";
    std::filesystem::create_directories(dir);
    return (dir / "theme.cfg").string();
}

namespace UITheme {

const UIPalette& Get() {
    return g_effectivePalette;
}

Font GetFont() {
    return WidgetFont::Get();
}

int PresetCount() {
    return kPresetCount;
}

const char* PresetName(int index) {
    if (index < 0 || index >= kPresetCount) return "";
    return kPresets[index].name;
}

int GetPresetIndex() {
    return g_presetIndex;
}

void SetPreset(int index) {
    if (index < 0 || index >= kPresetCount) return;
    g_presetIndex = index;
    g_basePalette = kPresets[index].make();
    ClearAllOverrides();
}

int FieldCount() {
    return kFieldCount;
}

const UIColorFieldInfo& Field(int index) {
    return kFields[index];
}

void SetOverride(int fieldIndex, Color value) {
    if (fieldIndex < 0 || fieldIndex >= kFieldCount) return;
    g_hasOverride[fieldIndex] = true;
    g_overrideValue[fieldIndex] = value;
    RecomputeEffectivePalette();
}

void ClearOverride(int fieldIndex) {
    if (fieldIndex < 0 || fieldIndex >= kFieldCount) return;
    g_hasOverride[fieldIndex] = false;
    RecomputeEffectivePalette();
}

void ClearAllOverrides() {
    for (int i = 0; i < kFieldCount; i++) g_hasOverride[i] = false;
    RecomputeEffectivePalette();
}

bool HasOverride(int fieldIndex) {
    if (fieldIndex < 0 || fieldIndex >= kFieldCount) return false;
    return g_hasOverride[fieldIndex];
}

void Save() {
    std::unordered_map<std::string, std::string> data;
    if (g_presetIndex >= 0) {
        data["preset"] = kPresets[g_presetIndex].name;
    } else {
        data["preset"] = "Custom";
        for (int i = 0; i < kFieldCount; i++) {
            Color c = g_basePalette.*(kFields[i].member);
            char hex[9];
            snprintf(hex, sizeof(hex), "%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
            data[std::string("base:") + kFields[i].name] = hex;
        }
    }
    for (int i = 0; i < kFieldCount; i++) {
        if (!g_hasOverride[i]) continue;
        Color c = g_overrideValue[i];
        char hex[9];
        snprintf(hex, sizeof(hex), "%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
        data[std::string("override:") + kFields[i].name] = hex;
    }
    LayoutSerializer::Save(GetThemeConfigPath(), data);
}

void Load() {
    std::unordered_map<std::string, std::string> data;
    if (!LayoutSerializer::Load(GetThemeConfigPath(), data)) return;

    auto presetIt = data.find("preset");
    if (presetIt != data.end()) {
        if (presetIt->second == "Custom") {
            g_presetIndex = -1;
            UIPalette custom = MakeDarkPalette();
            for (int i = 0; i < kFieldCount; i++) {
                auto it = data.find(std::string("base:") + kFields[i].name);
                if (it == data.end() || it->second.size() < 8) continue;
                unsigned long v = strtoul(it->second.c_str(), nullptr, 16);
                Color c = {(unsigned char)((v >> 24) & 0xFF), (unsigned char)((v >> 16) & 0xFF),
                           (unsigned char)((v >> 8) & 0xFF), (unsigned char)(v & 0xFF)};
                custom.*(kFields[i].member) = c;
            }
            g_basePalette = custom;
        } else {
            for (int i = 0; i < kPresetCount; i++) {
                if (presetIt->second == kPresets[i].name) {
                    g_presetIndex = i;
                    g_basePalette = kPresets[i].make();
                    break;
                }
            }
        }
    }

    ClearAllOverrides();
    for (int i = 0; i < kFieldCount; i++) {
        auto it = data.find(std::string("override:") + kFields[i].name);
        if (it == data.end() || it->second.size() < 8) continue;
        unsigned long v = strtoul(it->second.c_str(), nullptr, 16);
        Color c = {(unsigned char)((v >> 24) & 0xFF), (unsigned char)((v >> 16) & 0xFF),
                   (unsigned char)((v >> 8) & 0xFF), (unsigned char)(v & 0xFF)};
        g_hasOverride[i] = true;
        g_overrideValue[i] = c;
    }
    RecomputeEffectivePalette();
}

bool ExportToFile(const std::string& path) {
    std::unordered_map<std::string, std::string> data;
    for (int i = 0; i < kFieldCount; i++) {
        Color c = g_effectivePalette.*(kFields[i].member);
        char hex[9];
        snprintf(hex, sizeof(hex), "%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
        data[kFields[i].name] = hex;
    }
    return LayoutSerializer::Save(path, data);
}

bool ImportFromFile(const std::string& path) {
    std::unordered_map<std::string, std::string> data;
    if (!LayoutSerializer::Load(path, data)) return false;

    UIPalette custom = MakeDarkPalette();
    bool foundAny = false;
    for (int i = 0; i < kFieldCount; i++) {
        auto it = data.find(kFields[i].name);
        if (it == data.end() || it->second.size() < 8) continue;
        unsigned long v = strtoul(it->second.c_str(), nullptr, 16);
        Color c = {(unsigned char)((v >> 24) & 0xFF), (unsigned char)((v >> 16) & 0xFF),
                   (unsigned char)((v >> 8) & 0xFF), (unsigned char)(v & 0xFF)};
        custom.*(kFields[i].member) = c;
        foundAny = true;
    }
    if (!foundAny) return false;

    g_presetIndex = -1;
    g_basePalette = custom;
    ClearAllOverrides();
    return true;
}

}