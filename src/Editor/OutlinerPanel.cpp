#include "OutlinerPanel.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/SearchListElement.h"
#include "raylib.h"
#include <cstdlib>

void OutlinerPanel::Initialize() {
    name = "Outliner";
}

OutlinerPanel::~OutlinerPanel() {
    delete chrome;
}

void OutlinerPanel::Render() {
    if (!chrome) {
        chrome = new Widget();
        list = new SearchListElement();
        chrome->SetRoot(list);

        const UIPalette& theme = UITheme::Get();
        list->backgroundColor = theme.panelBg;
        list->rowColor = theme.textSecondary;
        list->rowHoveredColor = theme.surfaceHover;
        list->rowCurrentColor = theme.surfaceSelected;
        list->rowCurrentTextColor = theme.textPrimary;
        list->SetFieldColors(theme.fieldBg, theme.fieldBorder, theme.accent, theme.textPrimary, theme.textSecondary);

        list->onEntrySelected = [this](const std::string& id) {
            if (selectedActorIndex) *selectedActorIndex = atoi(id.c_str());
        };
    }

    std::vector<SearchListElement::Entry> entries;
    if (starMap) {
        const auto& instances = starMap->GetInstances();
        entries.reserve(instances.size());
        for (size_t i = 0; i < instances.size(); i++) {
            SearchListElement::Entry entry;
            entry.id = std::to_string(i);
            entry.label = instances[i].name;
            entry.isCurrent = selectedActorIndex && *selectedActorIndex == (int)i;
            entries.push_back(entry);
        }
    }
    list->SetEntries(entries);

    float dt = GetFrameTime();
    chrome->Tick(dt, bounds, 1.0f);
    Vector2 mouse = GetMousePosition();
    WidgetInput::ProcessInput(chrome, "Editor", dt, mouse, 1.0f);
    chrome->Draw(bounds, 1.0f);
}
