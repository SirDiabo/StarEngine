#include "WidgetHierarchyPanel.h"
#include "WidgetEditorSession.h"
#include "Editor/UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/VerticalBoxElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/TextElement.h"
#include "raylib.h"

namespace {
constexpr float kRowHeight = 24.0f;
constexpr float kIndentWidth = 14.0f;
}

void WidgetHierarchyPanel::Initialize() {
    name = "Hierarchy";
}

WidgetHierarchyPanel::~WidgetHierarchyPanel() {
    delete chrome;
}

void WidgetHierarchyPanel::AddElementRow(VerticalBoxElement* list, WidgetElement* element, int depth) {
    const UIPalette& theme = UITheme::Get();
    bool isHighlighted = session && session->IsHierarchyRowHighlighted(element);

    auto* row = new ButtonElement();
    row->size = { 0.0f, kRowHeight };
    row->defaultColor = isHighlighted ? theme.surfaceSelected : theme.panelBg;
    row->hoveredColor = isHighlighted ? theme.surfaceSelected : theme.surfaceHover;
    row->selectedColor = isHighlighted ? theme.surfaceSelected : theme.surfaceHover;
    row->pressedColor = theme.accent;
    row->onActivated = [this, element]() {
        if (session) session->SetSelectedElement(element);
    };

    auto* label = new TextElement();
    label->text = element->elementId + "  (" + element->GetClassName() + ")";
    label->color = theme.textPrimary;
    label->fontSize = 13.0f;
    label->anchor = { 0.0f, 0.5f };
    label->pivot = { 0.0f, 0.5f };
    label->offset = { 8.0f + depth * kIndentWidth, 0.0f };
    label->size = { 260.0f, 18.0f };
    row->AddChild(label);

    bool canDelete = element->parent != nullptr;
    if (canDelete) {
        auto* deleteBtn = new ButtonElement();
        deleteBtn->anchor = { 1.0f, 0.5f };
        deleteBtn->pivot = { 1.0f, 0.5f };
        deleteBtn->offset = { -4.0f, 0.0f };
        deleteBtn->size = { 18.0f, 18.0f };
        deleteBtn->defaultColor = theme.buttonBg;
        deleteBtn->hoveredColor = theme.danger;
        deleteBtn->selectedColor = theme.danger;
        deleteBtn->onActivated = [this, element]() {
            if (session) session->DeleteElement(element);
        };

        auto* deleteLabel = new TextElement();
        deleteLabel->text = "x";
        deleteLabel->color = theme.textPrimary;
        deleteLabel->fontSize = 12.0f;
        deleteLabel->anchor = { 0.5f, 0.5f };
        deleteLabel->pivot = { 0.5f, 0.5f };
        deleteLabel->size = { 18.0f, 18.0f };
        deleteBtn->AddChild(deleteLabel);

        row->AddChild(deleteBtn);
    }

    list->AddChild(row);
    rowEntries.push_back({ element, row });

    for (WidgetElement* child : element->GetChildren()) {
        AddElementRow(list, child, depth + 1);
    }
}

void WidgetHierarchyPanel::Render() {
    delete chrome;
    chrome = new Widget();
    rowEntries.clear();

    const UIPalette& theme = UITheme::Get();

    auto* list = new VerticalBoxElement();
    list->scrollEnabled = true;
    list->childSpacing = 1.0f;
    list->backgroundColor = theme.panelBg;
    list->size = { bounds.width, bounds.height };

    if (session && session->GetData().rootElement) {
        AddElementRow(list, session->GetData().rootElement, 0);
    }

    chrome->SetRoot(list);

    float dt = GetFrameTime();
    chrome->Tick(dt, bounds, 1.0f);

    Vector2 mouse = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        dragPending = false;
        dragCandidate = nullptr;
        bool clickedARow = false;
        for (RowEntry& entry : rowEntries) {
            if (CheckCollisionPointRec(mouse, entry.rowWidget->lastResolvedRect)) {
                clickedARow = true;
                dragPending = true;
                dragCandidate = entry.element;
                dragStartMouse = mouse;
                break;
            }
        }
        if (!clickedARow && session) session->ClearHierarchyHighlight();
    }

    if (dragPending && dragCandidate) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float dx = mouse.x - dragStartMouse.x;
            float dy = mouse.y - dragStartMouse.y;
            if (dx * dx + dy * dy > 36.0f) {
                if (dragCallback) dragCallback(dragCandidate->elementId, dragCandidate->GetClassName());
                dragPending = false;
                dragCandidate = nullptr;
            }
        } else {
            dragPending = false;
            dragCandidate = nullptr;
        }
    }

    WidgetInput::ProcessInput(chrome, "Editor", dt, mouse, 1.0f);
    chrome->Draw(bounds, 1.0f);
}
