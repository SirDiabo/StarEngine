#pragma once
#include "raylib.h"

namespace GraphCanvas {

    struct PanZoomState {
        Vector2 scroll{0, 0};
        float zoom = 1.0f;

        bool panning = false;
        Vector2 panStart{0, 0};
        Vector2 scrollAtPanStart{0, 0};
        bool didPan = false;
    };

    void UpdatePanZoom(PanZoomState& state, Rectangle area, Vector2 mousePos, bool allowZoom,
                        float minZoom = 0.1f, float maxZoom = 3.0f);

    Vector2 WorldToScreen(const PanZoomState& state, Rectangle area, Vector2 worldPos);
    Vector2 ScreenToWorld(const PanZoomState& state, Rectangle area, Vector2 screenPos);

    void DrawGrid(Rectangle area, Vector2 scroll, float zoom, float gridPitch = 16.0f,
                  Color bg = {24, 24, 26, 255}, Color dot = {55, 55, 60, 255});

    void DrawWire(Vector2 from, Vector2 to, float thickness, Color color);
    bool IsPointNearWire(Vector2 point, Vector2 from, Vector2 to);

    Rectangle NormalizeBox(Vector2 a, Vector2 b);
    Vector2 SnapToGrid(Vector2 worldPos, float gridSize = 16.0f);

}