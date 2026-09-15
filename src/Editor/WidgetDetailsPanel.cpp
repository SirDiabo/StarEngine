#include "WidgetDetailsPanel.h"
#include "WidgetEditorSession.h"
#include "Editor/UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/WidgetElementFields.h"
#include "Core/Widgets/VerticalBoxElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/SafeParse.h"
#include "raylib.h"
#include <cstdio>

namespace {
constexpr float kLabelHeight = 16.0f;
constexpr float kFieldHeight = 22.0f;

std::string FormatFloat(float v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}
}

void WidgetDetailsPanel::Initialize() {
    name = "Element Details";
}

WidgetDetailsPanel::~WidgetDetailsPanel() {
    delete chrome;
}

void WidgetDetailsPanel::AddStringField(VerticalBoxElement* list, const std::string& label,
    bool numericOnly,
    std::function<std::string(WidgetElement&)> getValue,
    std::function<void(WidgetElement&, const std::string&)> setValue) {

    const UIPalette& theme = UITheme::Get();

    auto* labelText = new TextElement();
    labelText->text = label;
    labelText->color = theme.textSecondary;
    labelText->fontSize = 11.0f;
    labelText->size = { 0.0f, kLabelHeight };
    labelText->offset = { 6.0f, 0.0f };
    list->AddChild(labelText);

    auto* input = new TextElement();
    input->editable = true;
    input->numericOnly = numericOnly;
    input->size = { 0.0f, kFieldHeight };
    input->color = theme.textPrimary;
    input->backgroundColor = theme.fieldBg;
    input->borderColor = theme.fieldBorder;
    input->fontSize = 13.0f;

    WidgetElement* selected = session->GetSelectedElement();
    if (selected) input->text = getValue(*selected);

    input->onTextCommitted = [this, getValue, setValue](const std::string& value) {
        WidgetElement* current = session ? session->GetSelectedElement() : nullptr;
        if (!current) return;
        setValue(*current, value);
        if (session) session->MarkDirty();
    };

    list->AddChild(input);

    FieldRow row;
    row.input = input;
    row.getValue = getValue;
    row.setValue = setValue;
    fieldRows.push_back(row);
}

void WidgetDetailsPanel::AddBoolField(VerticalBoxElement* list, const std::string& label,
    std::function<bool(WidgetElement&)> getValue,
    std::function<void(WidgetElement&, bool)> setValue) {

    const UIPalette& theme = UITheme::Get();
    WidgetElement* selected = session->GetSelectedElement();
    bool current = selected ? getValue(*selected) : false;

    auto* row = new ButtonElement();
    row->size = { 0.0f, kFieldHeight };
    row->defaultColor = current ? theme.accent : theme.buttonBg;
    row->hoveredColor = theme.buttonHover;
    row->pressedColor = theme.accent;

    auto* rowLabel = new TextElement();
    rowLabel->text = label + ": " + (current ? "true" : "false");
    rowLabel->color = theme.buttonText;
    rowLabel->fontSize = 12.0f;
    rowLabel->anchor = { 0.0f, 0.5f };
    rowLabel->pivot = { 0.0f, 0.5f };
    rowLabel->offset = { 8.0f, 0.0f };
    rowLabel->size = { 260.0f, 18.0f };
    row->AddChild(rowLabel);

    row->onActivated = [this, getValue, setValue, label, row, rowLabel]() {
        WidgetElement* element = session ? session->GetSelectedElement() : nullptr;
        if (!element) return;
        bool newValue = !getValue(*element);
        setValue(*element, newValue);
        const UIPalette& theme2 = UITheme::Get();
        row->defaultColor = newValue ? theme2.accent : theme2.buttonBg;
        rowLabel->text = label + ": " + (newValue ? "true" : "false");
        if (session) session->MarkDirty();
    };

    list->AddChild(row);
}

void WidgetDetailsPanel::RebuildForSelection(WidgetElement* element) {
    delete chrome;
    chrome = new Widget();
    fieldRows.clear();
    builtForElement = element;

    const UIPalette& theme = UITheme::Get();

    auto* list = new VerticalBoxElement();
    list->scrollEnabled = true;
    list->childSpacing = 2.0f;
    list->backgroundColor = theme.panelBg;

    chrome->SetRoot(list);

    if (!element) return;

    AddStringField(list, "Element Id", false,
        [](WidgetElement& e) { return e.elementId; },
        [](WidgetElement& e, const std::string& v) { e.elementId = v; });

    AddBoolField(list, "Visible",
        [](WidgetElement& e) { return e.visible; },
        [](WidgetElement& e, bool v) { e.visible = v; });

    AddBoolField(list, "Selectable (runtime)",
        [](WidgetElement& e) { return e.selectable; },
        [](WidgetElement& e, bool v) { e.selectable = v; });

    AddStringField(list, "Anchor X/Y", false,
        [](WidgetElement& e) { return FormatFloat(e.anchor.x) + "," + FormatFloat(e.anchor.y); },
        [](WidgetElement& e, const std::string& v) {
            float x = 0.0f, y = 0.0f;
            sscanf(v.c_str(), "%f,%f", &x, &y);
            e.anchor = { x, y };
        });

    AddStringField(list, "Pivot X/Y", false,
        [](WidgetElement& e) { return FormatFloat(e.pivot.x) + "," + FormatFloat(e.pivot.y); },
        [](WidgetElement& e, const std::string& v) {
            float x = 0.0f, y = 0.0f;
            sscanf(v.c_str(), "%f,%f", &x, &y);
            e.pivot = { x, y };
        });

    AddStringField(list, "Offset X/Y", false,
        [](WidgetElement& e) { return FormatFloat(e.offset.x) + "," + FormatFloat(e.offset.y); },
        [](WidgetElement& e, const std::string& v) {
            float x = 0.0f, y = 0.0f;
            sscanf(v.c_str(), "%f,%f", &x, &y);
            e.offset = { x, y };
        });

    AddStringField(list, "Size X/Y", false,
        [](WidgetElement& e) { return FormatFloat(e.size.x) + "," + FormatFloat(e.size.y); },
        [](WidgetElement& e, const std::string& v) {
            float x = 0.0f, y = 0.0f;
            sscanf(v.c_str(), "%f,%f", &x, &y);
            e.size = { x, y };
        });

    std::string className = element->GetClassName();
    for (const WidgetElementFieldDesc& desc : GetWidgetElementFields()) {
        if (className != desc.elementType) continue;

        if (desc.kind == WidgetFieldKind::Bool) {
            AddBoolField(list, desc.key,
                [desc](WidgetElement& e) { return desc.getValue(e) == "true"; },
                [desc](WidgetElement& e, bool v) { desc.setValue(e, v ? "true" : "false"); });
        } else {
            AddStringField(list, desc.key,
                desc.kind == WidgetFieldKind::Float || desc.kind == WidgetFieldKind::Int,
                desc.getValue, desc.setValue);
        }
    }
}

void WidgetDetailsPanel::Render() {
    WidgetElement* selected = session ? session->GetSelectedElement() : nullptr;

    if (selected != builtForElement || !chrome) {
        RebuildForSelection(selected);
    }

    if (!chrome) return;

    VerticalBoxElement* root = static_cast<VerticalBoxElement*>(chrome->rootElement);
    root->size = { bounds.width, bounds.height };

    if (selected) {
        for (FieldRow& row : fieldRows) {
            if (row.input->isEditing) continue;
            row.input->text = row.getValue(*selected);
        }
    }

    const UIPalette& theme = UITheme::Get();
    if (!selected) {
        DrawRectangleRec(bounds, theme.panelBg);
        DrawText("No element selected", (int)bounds.x + 10, (int)bounds.y + 10, 13, theme.textSecondary);
    }

    float dt = GetFrameTime();
    chrome->Tick(dt, bounds, 1.0f);
    WidgetInput::ProcessInput(chrome, "Editor", dt, GetMousePosition(), 1.0f);
    chrome->Draw(bounds, 1.0f);
}
