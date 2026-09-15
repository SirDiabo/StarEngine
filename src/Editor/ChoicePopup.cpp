#include "ChoicePopup.h"
#include "Core/Widgets/WidgetOverlay.h"
#include "Core/Widgets/SearchListElement.h"
#include "UITheme.h"
#include <cstdlib>

void ChoicePopup::Open(Vector2 anchorPos, float width, const std::vector<ChoiceOption>& choices) {
    open = true;
    hasPending = false;
    options = choices;

    float height = 4.0f;
    for (const ChoiceOption& opt : options) {
        height += (!opt.enabled && !opt.disabledReason.empty()) ? 34.0f : 20.0f;
    }
    area = { anchorPos.x, anchorPos.y, width, height };

    auto* list = new SearchListElement();
    list->SetShowSearchBar(false);
    list->rowHeight = 20.0f;
    const UIPalette& theme = UITheme::Get();
    list->backgroundColor = theme.panelBg;
    list->rowColor = theme.textPrimary;
    list->rowHoveredColor = theme.surfaceHover;
    list->rowDisabledColor = theme.buttonDisabledText;
    list->rowDisabledReasonColor = theme.textSecondary;

    std::vector<SearchListElement::Entry> entries;
    entries.reserve(options.size());
    for (size_t i = 0; i < options.size(); i++) {
        SearchListElement::Entry entry;
        entry.id = std::to_string(i);
        entry.label = options[i].label;
        entry.enabled = options[i].enabled;
        entry.disabledReason = options[i].disabledReason;
        entries.push_back(entry);
    }
    list->SetEntries(entries);

    list->onEntrySelected = [this](const std::string& id) {
        pendingIndex = atoi(id.c_str());
        hasPending = true;
    };

    WidgetOverlay::Open(list, { area.x, area.y }, { area.width, area.height }, [this]() {
        open = false;
    });
}

void ChoicePopup::Close() {
    open = false;
    WidgetOverlay::Close();
}

bool ChoicePopup::Render(int& outIndex) {
    if (!open) return false;
    if (hasPending) {
        outIndex = pendingIndex;
        hasPending = false;
        Close();
        return true;
    }
    return false;
}

void ChoicePopup::FlushDeferredDraws() {
}
