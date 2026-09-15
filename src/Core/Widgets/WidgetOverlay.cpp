#include "WidgetOverlay.h"
#include "Widget.h"
#include "WidgetElement.h"
#include "WidgetInput.h"

namespace {

Widget* g_overlayWidget = nullptr;
Rectangle g_overlayRect{};
std::function<void()> g_onClosed;
bool g_justOpened = false;

const char* kOverlayDevice = "Editor";

}

namespace WidgetOverlay {

void Open(WidgetElement* content, Vector2 anchorPos, Vector2 desiredSize, std::function<void()> onClosed) {
    Close();
    g_overlayWidget = new Widget();
    g_overlayWidget->SetRoot(content);
    g_overlayRect = { anchorPos.x, anchorPos.y, desiredSize.x, desiredSize.y };
    g_onClosed = onClosed;
    g_justOpened = true;
    WidgetInput::ClaimFocus(g_overlayWidget, kOverlayDevice);
}

void Close() {
    if (!g_overlayWidget) return;
    Widget* closing = g_overlayWidget;
    g_overlayWidget = nullptr;
    WidgetInput::ReleaseFocus(kOverlayDevice);
    delete closing;
    if (g_onClosed) {
        std::function<void()> cb = g_onClosed;
        g_onClosed = nullptr;
        cb();
    }
}

bool IsOpen() {
    return g_overlayWidget != nullptr;
}

void TickAndDraw(float deltaTime, Rectangle screenBounds) {
    if (!g_overlayWidget) return;

    if (g_overlayRect.x + g_overlayRect.width > screenBounds.x + screenBounds.width)
        g_overlayRect.x = screenBounds.x + screenBounds.width - g_overlayRect.width;
    if (g_overlayRect.y + g_overlayRect.height > screenBounds.y + screenBounds.height)
        g_overlayRect.y = screenBounds.y + screenBounds.height - g_overlayRect.height;
    if (g_overlayRect.x < screenBounds.x) g_overlayRect.x = screenBounds.x;
    if (g_overlayRect.y < screenBounds.y) g_overlayRect.y = screenBounds.y;

    bool closeRequested = false;
    if (!g_justOpened) {
        if (IsKeyPressed(KEY_ESCAPE)) closeRequested = true;
        bool clickedOutside = (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            && !CheckCollisionPointRec(GetMousePosition(), g_overlayRect);
        if (clickedOutside) closeRequested = true;
    }
    g_justOpened = false;

    if (closeRequested) {
        Close();
        return;
    }

    Vector2 mouse = GetMousePosition();
    g_overlayWidget->Tick(deltaTime, g_overlayRect, 1.0f);
    WidgetInput::ProcessInput(g_overlayWidget, kOverlayDevice, deltaTime, mouse, 1.0f);
    g_overlayWidget->Draw(g_overlayRect, 1.0f);
}

}
