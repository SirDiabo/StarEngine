#pragma once
#include "raylib.h"

class Panel;

namespace PanelCache {

struct Surface {
    RenderTexture2D target{};
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool valid = false;
};

bool CaptureScreenRect(Surface& surface, Rectangle area);
void DrawSurface(const Surface& surface);

void BeginFrame();
bool TryDraw(Panel* panel);
void Capture(Panel* panel);

}
