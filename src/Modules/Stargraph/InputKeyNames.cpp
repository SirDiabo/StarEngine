#include "InputKeyNames.h"
#include "raylib.h"
#include <cmath>
#include <cstdlib>
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
        {"GAMEPAD_FACE_UP", -7}, {"GAMEPAD_FACE_DOWN", -8},
        {"GAMEPAD_FACE_LEFT", -9}, {"GAMEPAD_FACE_RIGHT", -10},
        {"GAMEPAD_DPAD_UP", -11}, {"GAMEPAD_DPAD_DOWN", -12},
        {"GAMEPAD_DPAD_LEFT", -13}, {"GAMEPAD_DPAD_RIGHT", -14},
        {"GAMEPAD_SHOULDER_L", -15}, {"GAMEPAD_SHOULDER_R", -16},
        {"GAMEPAD_TRIGGER_L", -17}, {"GAMEPAD_TRIGGER_R", -18},
        {"GAMEPAD_SPECIAL_L", -19}, {"GAMEPAD_SPECIAL_R", -20},
        {"GAMEPAD_LEFT_THUMB", -21}, {"GAMEPAD_RIGHT_THUMB", -22},
        {"GAMEPAD_LEFT_STICK_X", -23}, {"GAMEPAD_LEFT_STICK_Y", -24},
        {"GAMEPAD_RIGHT_STICK_X", -25}, {"GAMEPAD_RIGHT_STICK_Y", -26},
        {"GAMEPAD_LEFT_STICK_UP", -27}, {"GAMEPAD_LEFT_STICK_DOWN", -28},
        {"GAMEPAD_LEFT_STICK_LEFT", -29}, {"GAMEPAD_LEFT_STICK_RIGHT", -30},
        {"GAMEPAD_RIGHT_STICK_UP", -31}, {"GAMEPAD_RIGHT_STICK_DOWN", -32},
        {"GAMEPAD_RIGHT_STICK_LEFT", -33}, {"GAMEPAD_RIGHT_STICK_RIGHT", -34},
        {"MOUSE_UP", -35}, {"MOUSE_DOWN", -36}, {"MOUSE_LEFT", -37}, {"MOUSE_RIGHT", -38},
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

bool InputKeyIsAnalog(const std::string& name) {
    static const std::vector<std::string> analogKeys = {
        "MOUSE_WHEEL", "MOUSE_X", "MOUSE_Y",
        "GAMEPAD_LEFT_STICK_X", "GAMEPAD_LEFT_STICK_Y",
        "GAMEPAD_RIGHT_STICK_X", "GAMEPAD_RIGHT_STICK_Y",
        "GAMEPAD_TRIGGER_L", "GAMEPAD_TRIGGER_R",
    };
    for (const std::string& k : analogKeys) if (k == name) return true;
    return false;
}

bool PollInputKey(const std::string& keyName, float& outAxis, int gamepadIndex) {
    outAxis = 0.0f;
    bool held = false;

    if (keyName == "LEFT_MOUSE") {
        held = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    } else if (keyName == "RIGHT_MOUSE") {
        held = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    } else if (keyName == "MIDDLE_MOUSE") {
        held = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    } else if (keyName == "MOUSE_WHEEL") {
        outAxis = GetMouseWheelMove();
        if (outAxis > 1.0f) outAxis = 1.0f;
        if (outAxis < -1.0f) outAxis = -1.0f;
        held = outAxis != 0.0f;
    } else if (keyName == "MOUSE_X") {
        outAxis = GetMouseDelta().x * -1.0f;
        held = outAxis != 0.0f;
    } else if (keyName == "MOUSE_Y") {
        outAxis = GetMouseDelta().y * -1.0f;
        held = outAxis != 0.0f;
    } else if (keyName == "MOUSE_UP") {
        held = GetMouseDelta().y < 0.0f;
    } else if (keyName == "MOUSE_DOWN") {
        held = GetMouseDelta().y > 0.0f;
    } else if (keyName == "MOUSE_LEFT") {
        held = GetMouseDelta().x < 0.0f;
    } else if (keyName == "MOUSE_RIGHT") {
        held = GetMouseDelta().x > 0.0f;
    } else if (keyName.rfind("GAMEPAD_", 0) == 0) {
        const int pad = gamepadIndex;
        bool avail = IsGamepadAvailable(pad);
        if (keyName == "GAMEPAD_FACE_UP")         held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_UP);
        else if (keyName == "GAMEPAD_FACE_DOWN")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        else if (keyName == "GAMEPAD_FACE_LEFT")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        else if (keyName == "GAMEPAD_FACE_RIGHT") held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        else if (keyName == "GAMEPAD_DPAD_UP")    held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_UP);
        else if (keyName == "GAMEPAD_DPAD_DOWN")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        else if (keyName == "GAMEPAD_DPAD_LEFT")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        else if (keyName == "GAMEPAD_DPAD_RIGHT") held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        else if (keyName == "GAMEPAD_SHOULDER_L") held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
        else if (keyName == "GAMEPAD_SHOULDER_R") held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
        else if (keyName == "GAMEPAD_SPECIAL_L")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_MIDDLE_LEFT);
        else if (keyName == "GAMEPAD_SPECIAL_R")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_MIDDLE_RIGHT);
        else if (keyName == "GAMEPAD_LEFT_THUMB")  held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_THUMB);
        else if (keyName == "GAMEPAD_RIGHT_THUMB") held = avail && IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_THUMB);
        else if (keyName == "GAMEPAD_LEFT_STICK_X")  { outAxis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_X)  : 0.0f; held = fabsf(outAxis) > 0.1f; }
        else if (keyName == "GAMEPAD_LEFT_STICK_Y")  { outAxis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_Y)  : 0.0f; held = fabsf(outAxis) > 0.1f; }
        else if (keyName == "GAMEPAD_RIGHT_STICK_X") { outAxis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_RIGHT_X) : 0.0f; held = fabsf(outAxis) > 0.1f; }
        else if (keyName == "GAMEPAD_RIGHT_STICK_Y") { outAxis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_RIGHT_Y) : 0.0f; held = fabsf(outAxis) > 0.1f; }
        else if (keyName == "GAMEPAD_TRIGGER_L") { outAxis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_TRIGGER)  : 0.0f; held = outAxis > 0.1f; }
        else if (keyName == "GAMEPAD_TRIGGER_R") { outAxis = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_RIGHT_TRIGGER) : 0.0f; held = outAxis > 0.1f; }
        else if (keyName == "GAMEPAD_LEFT_STICK_UP")     { float y = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_Y)  : 0.0f; held = y < -0.1f; }
        else if (keyName == "GAMEPAD_LEFT_STICK_DOWN")   { float y = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_Y)  : 0.0f; held = y > 0.1f; }
        else if (keyName == "GAMEPAD_LEFT_STICK_LEFT")   { float x = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_X)  : 0.0f; held = x < -0.1f; }
        else if (keyName == "GAMEPAD_LEFT_STICK_RIGHT")  { float x = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_X)  : 0.0f; held = x > 0.1f; }
        else if (keyName == "GAMEPAD_RIGHT_STICK_UP")    { float y = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_RIGHT_Y) : 0.0f; held = y < -0.1f; }
        else if (keyName == "GAMEPAD_RIGHT_STICK_DOWN")  { float y = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_RIGHT_Y) : 0.0f; held = y > 0.1f; }
        else if (keyName == "GAMEPAD_RIGHT_STICK_LEFT")  { float x = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_RIGHT_X) : 0.0f; held = x < -0.1f; }
        else if (keyName == "GAMEPAD_RIGHT_STICK_RIGHT") { float x = avail ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_RIGHT_X) : 0.0f; held = x > 0.1f; }
    } else {
        int key = InputKeyNameToCode(keyName);
        held = key != 0 && IsKeyDown(key);
    }

    return held;
}

bool InputKeyBelongsToDevice(const std::string& keyName, const std::string& device, int& outGamepadIndex) {
    bool isGamepadKey = keyName.rfind("GAMEPAD_", 0) == 0;

    if (device == "Keyboard+Mouse") return !isGamepadKey;

    if (device.rfind("Gamepad ", 0) == 0) {
        if (!isGamepadKey) return false;
        outGamepadIndex = atoi(device.c_str() + 8);
        return true;
    }

    return false; // Unclaimed, or unrecognized device: no input
}