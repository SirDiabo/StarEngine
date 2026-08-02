#include "UITheme.h"
#include "Core/LayoutSerializer.h"
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
    data["preset"] = kPresets[g_presetIndex].name;
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
        for (int i = 0; i < kPresetCount; i++) {
            if (presetIt->second == kPresets[i].name) {
                g_presetIndex = i;
                g_basePalette = kPresets[i].make();
                break;
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

}