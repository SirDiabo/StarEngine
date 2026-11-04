#pragma once
#include <string>

namespace AudioConfig {

std::string BuildPakText();
void LoadPakText(const std::string& text);
void LoadLoose(const std::string& directory);

}
