#include "GraphCanvas.h"
#include <cmath>

namespace GraphCanvas {

void UpdatePanZoom(PanZoomState& state, Rectangle area, Vector2 mousePos, bool allowZoom, float minZoom, float maxZoom) {
    float wheel = GetMouseWheelMove();
    if (allowZoom && wheel != 0 && CheckCollisionPointRec(mousePos, area)) {
        float newZoom = state.zoom + wheel * 0.1f;
        newZoom = fmaxf(minZoom, fminf(maxZoom, newZoom));

        Vector2 worldBeforeZoom = ScreenToWorld(state, area, mousePos);
        state.zoom = newZoom;
        state.scroll.x = mousePos.x - area.x - worldBeforeZoom.x * state.zoom;
        state.scroll.y = mousePos.y - area.y - worldBeforeZoom.y * state.zoom;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && CheckCollisionPointRec(mousePos, area)) {
        state.panning = true;
        state.panStart = mousePos;
        state.scrollAtPanStart = state.scroll;
        state.didPan = false;
    }
    if (state.panning) {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            state.scroll.x = state.scrollAtPanStart.x + (mousePos.x - state.panStart.x);
            state.scroll.y = state.scrollAtPanStart.y + (mousePos.y - state.panStart.y);
            if (state.panStart.x != mousePos.x || state.panStart.y != mousePos.y) {
                state.didPan = true;
            }
        } else {
            state.panning = false;
        }
    }
}

Vector2 WorldToScreen(const PanZoomState& state, Rectangle area, Vector2 worldPos) {
    return {area.x + worldPos.x * state.zoom + state.scroll.x,
            area.y + worldPos.y * state.zoom + state.scroll.y};
}

Vector2 ScreenToWorld(const PanZoomState& state, Rectangle area, Vector2 screenPos) {
    return {(screenPos.x - area.x - state.scroll.x) / state.zoom,
            (screenPos.y - area.y - state.scroll.y) / state.zoom};
}

void DrawGrid(Rectangle area, Vector2 scroll, float zoom, float gridPitch, Color bg, Color dot) {
    DrawRectangleRec(area, bg);

    float stepX = gridPitch * zoom;
    float stepY = gridPitch * zoom;
    float startX = fmodf(scroll.x, stepX) + area.x;
    float startY = fmodf(scroll.y, stepY) + area.y;
    for (float gx = startX; gx < area.x + area.width; gx += stepX) {
        for (float gy = startY; gy < area.y + area.height; gy += stepY) {
            DrawPixel((int)gx, (int)gy, dot);
        }
    }
}

void DrawWire(Vector2 from, Vector2 to, float thickness, Color color) {
    DrawLineBezier(from, to, thickness, color);
}

bool IsPointNearWire(Vector2 point, Vector2 from, Vector2 to) {
    Vector2 mid = {(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f};
    if (CheckCollisionPointCircle(point, mid, 8.0f) ||
        CheckCollisionPointCircle(point, from, 12.0f) ||
        CheckCollisionPointCircle(point, to, 12.0f)) {
        return true;
    }

    for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
        float cx1 = from.x + (to.x - from.x) * 0.5f;
        float cy1 = from.y;
        float cx2 = cx1;
        float cy2 = to.y;
        float bx = (1 - t) * (1 - t) * (1 - t) * from.x + 3 * (1 - t) * (1 - t) * t * cx1 +
                   3 * (1 - t) * t * t * cx2 + t * t * t * to.x;
        float by = (1 - t) * (1 - t) * (1 - t) * from.y + 3 * (1 - t) * (1 - t) * t * cy1 +
                   3 * (1 - t) * t * t * cy2 + t * t * t * to.y;
        if (CheckCollisionPointCircle(point, {bx, by}, 7.0f)) return true;
    }
    return false;
}

Rectangle NormalizeBox(Vector2 a, Vector2 b) {
    float minX = (a.x < b.x) ? a.x : b.x;
    float minY = (a.y < b.y) ? a.y : b.y;
    float maxX = (a.x > b.x) ? a.x : b.x;
    float maxY = (a.y > b.y) ? a.y : b.y;
    return {minX, minY, maxX - minX, maxY - minY};
}

Vector2 SnapToGrid(Vector2 worldPos, float gridSize) {
    return {roundf(worldPos.x / gridSize) * gridSize, roundf(worldPos.y / gridSize) * gridSize};
}

}