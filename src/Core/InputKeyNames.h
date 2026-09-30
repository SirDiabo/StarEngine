#pragma once
#include <string>
#include <vector>

int InputKeyNameToCode(const std::string& name);
std::string InputKeyCodeToName(int code);
std::vector<std::string> GetInputKeyNames();

bool InputKeyIsAnalog(const std::string& name);

bool PollInputKey(const std::string& name, float& outAxis, int gamepadIndex = 0);

bool InputKeyBelongsToDevice(const std::string& keyName, const std::string& device, int& outGamepadIndex);

bool IsCtrlKeyDown();
bool IsAltKeyDown();
bool IsShiftKeyDown();
bool InputKeyCodeIsModifier(int keyCode);
