#pragma once
#include <string>
#include "raylib.h"

class Widget;

namespace WidgetInput {

void ProcessInput(Widget* widget, const std::string& device, float deltaTime, Vector2 localMousePos);
void ClaimFocus(Widget* widget, const std::string& device);
bool IsFocused(const Widget* widget, const std::string& device);

}
