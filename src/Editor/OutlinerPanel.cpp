#include "OutlinerPanel.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/VerticalBoxElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/TextElement.h"
#include "raylib.h"

namespace {
constexpr float kRowHeight = 20.0f;
}

void OutlinerPanel::Initialize() {
    name = "Outliner";
}

OutlinerPanel::~OutlinerPanel() {
    delete chrome;
}

void OutlinerPanel::Render() {
    delete chrome;
    chrome = new Widget();

    const UIPalette& theme = UITheme::Get();

    auto* list = new VerticalBoxElement();
    list->scrollEnabled = true;
    list->backgroundColor = theme.panelBg;
    list->size = { bounds.width, bounds.height };
    chrome->SetRoot(list);

    if (!starMap) {
        auto* empty = new TextElement();
        empty->text = "No map loaded";
        empty->color = theme.textSecondary;
        empty->fontSize = 11.0f;
        empty->anchor = {0.0f, 0.0f};
        empty->pivot = {0.0f, 0.0f};
        empty->offset = {8.0f, 4.0f};
        empty->size = {0.0f, 18.0f};
        list->AddChild(empty);
    } else {
        const auto& instances = starMap->GetInstances();
        for (size_t i = 0; i < instances.size(); i++) {
            bool isSelected = selectedActorIndex && *selectedActorIndex == (int)i;

            auto* row = new ButtonElement();
            row->size = { 0.0f, kRowHeight };
            row->defaultColor = isSelected ? theme.surfaceSelected : theme.panelBg;
            row->hoveredColor = isSelected ? theme.surfaceSelected : theme.surfaceHover;
            row->selectedColor = isSelected ? theme.surfaceSelected : theme.surfaceHover;
            row->pressedColor = theme.accent;
            int idx = (int)i;
            row->onActivated = [this, idx]() {
                if (selectedActorIndex) *selectedActorIndex = idx;
            };

            auto* label = new TextElement();
            label->text = instances[i].name;
            label->color = isSelected ? theme.textPrimary : theme.textSecondary;
            label->fontSize = 11.0f;
            label->anchor = {0.0f, 0.5f};
            label->pivot = {0.0f, 0.5f};
            label->offset = {8.0f, 0.0f};
            label->size = {0.0f, 16.0f};
            row->AddChild(label);

            list->AddChild(row);
        }
    }

    float dt = GetFrameTime();
    chrome->Tick(dt, bounds, 1.0f);
    Vector2 mouse = GetMousePosition();
    WidgetInput::ProcessInput(chrome, "Editor", dt, mouse, 1.0f);
    chrome->Draw(bounds, 1.0f);
}
