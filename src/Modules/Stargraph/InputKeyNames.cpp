#include "InputKeyNames.h"
#include "raylib.h"
#include <utility>
#include <vector>

namespace {

const std::vector<std::pair<std::string, int>>& Table() {
    static const std::vector<std::pair<std::string, int>> table = {
        {"A", KEY_A}, {"B", KEY_B}, {"C", KEY_C}, {"D", KEY_D}, {"E", KEY_E},
        {"F", KEY_F}, {"G", KEY_G}, {"H", KEY_H}, {"I", KEY_I}, {"J", KEY_J},
        {"K", KEY_K}, {"L", KEY_L}, {"M", KEY_M}, {"N", KEY_N}, {"O", KEY_O},
        {"P", KEY_P}, {"Q", KEY_Q}, {"R", KEY_R}, {"S", KEY_S}, {"T", KEY_T},
        {"U", KEY_U}, {"V", KEY_V}, {"W", KEY_W}, {"X", KEY_X}, {"Y", KEY_Y},
        {"Z", KEY_Z},
        {"0", KEY_ZERO}, {"1", KEY_ONE}, {"2", KEY_TWO}, {"3", KEY_THREE},
        {"4", KEY_FOUR}, {"5", KEY_FIVE}, {"6", KEY_SIX}, {"7", KEY_SEVEN},
        {"8", KEY_EIGHT}, {"9", KEY_NINE},
        {"SPACE", KEY_SPACE}, {"ENTER", KEY_ENTER}, {"ESCAPE", KEY_ESCAPE},
        {"TAB", KEY_TAB}, {"BACKSPACE", KEY_BACKSPACE},
        {"LEFT_SHIFT", KEY_LEFT_SHIFT}, {"RIGHT_SHIFT", KEY_RIGHT_SHIFT},
        {"LEFT_CTRL", KEY_LEFT_CONTROL}, {"RIGHT_CTRL", KEY_RIGHT_CONTROL},
        {"LEFT_ALT", KEY_LEFT_ALT}, {"RIGHT_ALT", KEY_RIGHT_ALT},
        {"UP", KEY_UP}, {"DOWN", KEY_DOWN}, {"LEFT", KEY_LEFT}, {"RIGHT", KEY_RIGHT},
        {"F1", KEY_F1}, {"F2", KEY_F2}, {"F3", KEY_F3}, {"F4", KEY_F4},
        {"F5", KEY_F5}, {"F6", KEY_F6}, {"F7", KEY_F7}, {"F8", KEY_F8},
        {"F9", KEY_F9}, {"F10", KEY_F10}, {"F11", KEY_F11}, {"F12", KEY_F12},
        {"LEFT_MOUSE", -1}, {"RIGHT_MOUSE", -2}, {"MIDDLE_MOUSE", -3},
        {"MOUSE_WHEEL", -4}, {"MOUSE_X", -5}, {"MOUSE_Y", -6},
    };
    return table;
}}

int InputKeyNameToCode(const std::string& name) {
    for (const auto& entry : Table()) {
        if (entry.first == name) return entry.second;
    }
    return -1;
}

std::string InputKeyCodeToName(int code) {
    for (const auto& entry : Table()) {
        if (entry.second == code) return entry.first;
    }
    return "";
}

std::vector<std::string> GetInputKeyNames() {
    std::vector<std::string> names;
    for (const auto& entry : Table()) names.push_back(entry.first);
    return names;
}