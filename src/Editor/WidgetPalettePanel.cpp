#include "WidgetPalettePanel.h"
#include "WidgetEditorSession.h"
#include "Editor/UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/VerticalBoxElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/WidgetElementFields.h"
#include "raylib.h"

namespace {
constexpr float kRowHeight = 26.0f;
}

void WidgetPalettePanel::Initialize() {
    name = "Palette";
}

WidgetPalettePanel::~WidgetPalettePanel() {
    delete chrome;
}

void WidgetPalettePanel::BuildChrome() {
    delete chrome;
    chrome = new Widget();

    const UIPalette& theme = UITheme::Get();

    auto* list = new VerticalBoxElement();
    list->scrollEnabled = true;
    list->childSpacing = 2.0f;
    list->backgroundColor = theme.panelBg;

    for (const WidgetElementTypeDesc& typeDesc : GetWidgetElementTypes()) {
        std::string className = typeDesc.elementType;

        auto* row = new ButtonElement();
        row->size = { 0.0f, kRowHeight };
        row->defaultColor = theme.buttonBg;
        row->hoveredColor = theme.buttonHover;
        row->selectedColor = theme.buttonHover;
        row->pressedColor = theme.accent;
        row->onActivated = [this, className]() {
            if (!session) return;
            session->AddElement(className, session->GetSelectedElement());
        };

        auto* label = new TextElement();
        label->text = className;
        label->color = theme.buttonText;
        label->fontSize = 13.0f;
        label->anchor = { 0.0f, 0.5f };
        label->pivot = { 0.0f, 0.5f };
        label->offset = { 8.0f, 0.0f };
        label->size = { 260.0f, 18.0f };

        row->AddChild(label);
        list->AddChild(row);
    }

    chrome->SetRoot(list);
}

void WidgetPalettePanel::Render() {
    if (!chrome) BuildChrome();

    VerticalBoxElement* root = static_cast<VerticalBoxElement*>(chrome->rootElement);
    root->size = { bounds.width, bounds.height };

    float dt = GetFrameTime();
    chrome->Tick(dt, bounds, 1.0f);
    WidgetInput::ProcessInput(chrome, "Editor", dt, GetMousePosition(), 1.0f);
    chrome->Draw(bounds, 1.0f);
}
