#pragma once
#include "raylib.h"

namespace WidgetRenderer {

void DrawScreenSpaceWidgets(Rectangle targetRect, bool nativeResolutionOnly, float bufferScale = 1.0f);
void TickScreenSpaceWidgets(float deltaTime, Rectangle targetRect, Vector2 localMousePos, bool nativeResolutionOnly, float bufferScale = 1.0f);

}
