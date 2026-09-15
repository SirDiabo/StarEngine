#pragma once
#include <string>
#include "raylib.h"

class Widget;

namespace WidgetInput {

void ProcessInput(Widget* widget, const std::string& device, float deltaTime, Vector2 localMousePos, float uiScale = 1.0f);
void ClaimFocus(Widget* widget, const std::string& device);
void ReleaseFocus(const std::string& device);
bool IsFocused(const Widget* widget, const std::string& device);

}
