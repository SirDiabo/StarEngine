#include "SliderElement.h"
#include "raylib.h"

void SliderElement::ApplyMouseX(float mouseX, Rectangle rect) {
    float t = (rect.width > 0.0f) ? (mouseX - rect.x) / rect.width : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float newValue = minValue + t * (maxValue - minValue);
    if (newValue != value) {
        value = newValue;
        if (onValueChanged) onValueChanged(value);
    }
}

void SliderElement::OnConfirmed() {
    dragging = true;
    ApplyMouseX((float)GetMouseX(), lastResolvedRect);
    WidgetElement::OnConfirmed();
}

void SliderElement::OnTickSelf(float deltaTime, Rectangle rect, float uiScale) {
    if (!dragging) return;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        ApplyMouseX((float)GetMouseX(), rect);
    } else {
        dragging = false;
    }
}

void SliderElement::OnDrawSelf(Rectangle rect, float uiScale) {
    if (!gradientStops.empty()) {
        int n = (int)gradientStops.size();
        if (n == 1) {
            DrawRectangleRec(rect, gradientStops[0]);
        } else {
            float segW = rect.width / (float)(n - 1);
            for (int i = 0; i < n - 1; i++) {
                Rectangle seg = { rect.x + segW * i, rect.y, segW + 1.0f, rect.height };
                DrawRectangleGradientEx(seg, gradientStops[i], gradientStops[i], gradientStops[i + 1], gradientStops[i + 1]);
            }
        }
    } else {
        DrawRectangleRec(rect, trackColor);
        float fillT = (maxValue > minValue) ? (value - minValue) / (maxValue - minValue) : 0.0f;
        Rectangle fill = { rect.x, rect.y, rect.width * fillT, rect.height };
        DrawRectangleRec(fill, fillColor);
    }
    DrawRectangleLinesEx(rect, 1.0f, borderColor);

    float t = (maxValue > minValue) ? (value - minValue) / (maxValue - minValue) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float handleW = 4.0f * uiScale;
    Rectangle handle = { rect.x + t * rect.width - handleW * 0.5f, rect.y - 1.0f, handleW, rect.height + 2.0f };
    DrawRectangleRec(handle, handleColor);
}
