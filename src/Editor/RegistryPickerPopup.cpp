#include "RegistryPickerPopup.h"
#include "Core/NamedListRegistry.h"
#include "Core/Widgets/WidgetOverlay.h"
#include "Core/Widgets/SearchListElement.h"
#include "UITheme.h"
#include <algorithm>

void RegistryPickerPopup::Open(Vector2 anchorPos, const std::string& cat) {
    open = true;
    hasPending = false;
    category = cat;
    entries = NamedListRegistry::Get().GetList(category);

    float rowH = 20.0f;
    float contentHeight = (float)(entries.size() + 2) * rowH;
    float maxContentHeight = 260.0f;
    float height = 26.0f + std::min(contentHeight, maxContentHeight) + 4.0f;
    area = { anchorPos.x, anchorPos.y, 200.0f, height };

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    if (area.x + area.width > screenW) area.x = screenW - area.width;
    if (area.x < 0) area.x = 0;
    if (area.y + area.height > screenH) area.y = screenH - area.height;
    if (area.y < 0) area.y = 0;

    auto* list = new SearchListElement();
    list->rowHeight = rowH;
    list->allowCreate = true;
    list->createLabel = "+ New...";
    const UIPalette& theme = UITheme::Get();
    list->backgroundColor = theme.panelBg;
    list->rowColor = theme.textPrimary;
    list->rowHoveredColor = theme.surfaceHover;
    list->createRowColor = theme.success;
    list->SetFieldColors(theme.fieldBg, theme.fieldBorder, theme.accent, theme.textPrimary, theme.textSecondary);

    std::vector<SearchListElement::Entry> listEntries;
    listEntries.reserve(entries.size() + 1);

    SearchListElement::Entry noneEntry;
    noneEntry.id = "";
    noneEntry.label = "(None)";
    listEntries.push_back(noneEntry);

    for (const std::string& name : entries) {
        SearchListElement::Entry entry;
        entry.id = name;
        entry.label = name;
        listEntries.push_back(entry);
    }
    list->SetEntries(listEntries);

    list->onEntrySelected = [this](const std::string& id) {
        pendingValue = id;
        hasPending = true;
    };
    list->onCreateRequested = [this](const std::string& name) {
        NamedListRegistry::Get().AddEntry(category, name);
        pendingValue = name;
        hasPending = true;
    };

    WidgetOverlay::Open(list, { area.x, area.y }, { area.width, area.height }, [this]() {
        open = false;
    });
}

void RegistryPickerPopup::Close() {
    open = false;
    WidgetOverlay::Close();
}

bool RegistryPickerPopup::Render(std::string& outVal) {
    if (!open) return false;
    if (hasPending) {
        outVal = pendingValue;
        hasPending = false;
        Close();
        return true;
    }
    return false;
}

void RegistryPickerPopup::FlushDeferredDraws() {
}
