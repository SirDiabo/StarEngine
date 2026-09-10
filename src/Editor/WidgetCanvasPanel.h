#pragma once
#include "Core/Panel.h"
#include "raylib.h"

class WidgetEditorSession;
class WidgetElement;

class WidgetCanvasPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

    void SetSession(WidgetEditorSession* s) { session = s; }

private:
    WidgetElement* HitTest(WidgetElement* element, Vector2 point) const;

    WidgetEditorSession* session = nullptr;
    bool dragging = false;
    Vector2 dragStartMouse{ 0.0f, 0.0f };
    Vector2 dragStartOffset{ 0.0f, 0.0f };
};
