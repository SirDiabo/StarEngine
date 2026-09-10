#include "WidgetCanvasPanel.h"
#include "WidgetEditorSession.h"
#include "Editor/UITheme.h"
#include "Core/Widgets/WidgetElement.h"

void WidgetCanvasPanel::Initialize() {
    name = "Canvas";
}

WidgetElement* WidgetCanvasPanel::HitTest(WidgetElement* element, Vector2 point) const {
    if (!element || !element->visible) return nullptr;
    const std::vector<WidgetElement*>& children = element->GetChildren();
    for (int i = (int)children.size() - 1; i >= 0; i--) {
        WidgetElement* hit = HitTest(children[i], point);
        if (hit) return hit;
    }
    if (CheckCollisionPointRec(point, element->lastResolvedRect)) return element;
    return nullptr;
}

void WidgetCanvasPanel::Render() {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(bounds, theme.windowBg);

    if (!session || !session->GetData().rootElement) {
        DrawText("No elements yet - add one from the Palette", (int)bounds.x + 12, (int)bounds.y + 12, 14, theme.textSecondary);
        return;
    }

    WidgetElement* root = session->GetData().rootElement;
    float uiScale = session->GetData().uiScale;
    if (uiScale <= 0.0f) uiScale = 1.0f;

    root->Tick(0.0f, bounds, uiScale);
    root->Draw(bounds, uiScale);

    Vector2 mouse = GetMousePosition();
    bool insideCanvas = CheckCollisionPointRec(mouse, bounds);

    if (insideCanvas && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        WidgetElement* hit = HitTest(root, mouse);
        session->SetSelectedElement(hit);
        if (hit) {
            dragging = true;
            dragStartMouse = mouse;
            dragStartOffset = hit->offset;
        }
    }

    if (dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            WidgetElement* selected = session->GetSelectedElement();
            if (selected) {
                Vector2 delta = { (mouse.x - dragStartMouse.x) / uiScale, (mouse.y - dragStartMouse.y) / uiScale };
                selected->offset = { dragStartOffset.x + delta.x, dragStartOffset.y + delta.y };
                session->MarkDirty();
            }
        } else {
            dragging = false;
        }
    }

    WidgetElement* selected = session->GetSelectedElement();
    if (selected) {
        DrawRectangleLinesEx(selected->lastResolvedRect, 2.0f, theme.accent);
    }
}
