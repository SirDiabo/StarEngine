#pragma once
#include "raylib.h"

namespace WidgetRenderer {

void DrawScreenSpaceWidgets(Rectangle targetRect, bool nativeResolutionOnly);
void TickScreenSpaceWidgets(float deltaTime, Rectangle targetRect, Vector2 localMousePos, bool nativeResolutionOnly);

}
