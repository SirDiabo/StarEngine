#pragma once
#include "raylib.h"
#include <functional>

class WidgetElement;

namespace WidgetOverlay {

struct FrameResult {
    Rectangle content{};
    bool closeRequested = false;
};

void Open(WidgetElement* content, Vector2 anchorPos, Vector2 desiredSize, std::function<void()> onClosed = nullptr, bool dimBackground = false);
void OpenFramed(WidgetElement* content, Vector2 contentSize, std::function<FrameResult()> drawFrame,
                std::function<void()> onClosed = nullptr, std::function<void()> onCloseRequested = nullptr);
void Close();
bool IsOpen();
void TickAndDraw(float deltaTime, Rectangle screenBounds);

}
