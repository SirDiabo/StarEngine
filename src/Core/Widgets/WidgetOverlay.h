#pragma once
#include "raylib.h"
#include <functional>

class WidgetElement;

namespace WidgetOverlay {

void Open(WidgetElement* content, Vector2 anchorPos, Vector2 desiredSize, std::function<void()> onClosed = nullptr, bool dimBackground = false);
void Close();
bool IsOpen();
void TickAndDraw(float deltaTime, Rectangle screenBounds);

}
