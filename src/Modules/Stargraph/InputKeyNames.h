#pragma once
#include <string>
#include <vector>

int InputKeyNameToCode(const std::string& name);
std::string InputKeyCodeToName(int code);
std::vector<std::string> GetInputKeyNames();