#include "SearchListElement.h"
#include "TextElement.h"
#include "BoxElement.h"
#include "ButtonElement.h"
#include "raylib.h"
#include <algorithm>

namespace {

std::string ToLowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return (char)tolower(c); });
    return out;
}

bool EntriesEqual(const std::vector<SearchListElement::Entry>& a, const std::vector<SearchListElement::Entry>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].id != b[i].id || a[i].label != b[i].label || a[i].category != b[i].category ||
            a[i].enabled != b[i].enabled || a[i].disabledReason != b[i].disabledReason || a[i].isCurrent != b[i].isCurrent) {
            return false;
        }
    }
    return true;
}

}

SearchListElement::SearchListElement() {
    searchBox = new TextElement();
    searchBox->editable = true;
    searchBox->text = "";
    searchBox->placeholder = "Search...";
    searchBox->fontSize = 13.0f;
    searchBox->onTextChanged = [this](const std::string&) {
        listBox->scrollOffset = 0.0f;
        RebuildRows();
    };
    AddChild(searchBox);

    listBox = new BoxElement();
    listBox->axis = BoxAxis::Vertical;
    listBox->scrollEnabled = true;
    listBox->backgroundColor = { 0, 0, 0, 0 };
    AddChild(listBox);

    createBox = new TextElement();
    createBox->editable = true;
    createBox->text = "";
    createBox->placeholder = "Name...";
    createBox->fontSize = 13.0f;
    createBox->visible = false;
    createBox->onTextCommitted = [this](const std::string& text) {
        if (!text.empty() && onCreateRequested) onCreateRequested(text);
        ExitCreateMode();
    };
    createBox->onEditCancelled = [this]() { ExitCreateMode(); };
    AddChild(createBox);
}

void SearchListElement::SetEntries(std::vector<Entry> newEntries) {
    if (EntriesEqual(entries, newEntries)) return;
    entries = std::move(newEntries);
    RebuildRows();
}

void SearchListElement::SetShowSearchBar(bool show) {
    showSearchBar = show;
    searchBox->visible = show;
    RebuildRows();
}

void SearchListElement::SetFieldColors(Color fieldBg, Color fieldBorder, Color fieldEditingBorder, Color fieldText, Color fieldPlaceholder) {
    searchBox->backgroundColor = fieldBg;
    searchBox->borderColor = fieldBorder;
    searchBox->editingBorderColor = fieldEditingBorder;
    searchBox->color = fieldText;
    searchBox->placeholderColor = fieldPlaceholder;
    createBox->backgroundColor = fieldBg;
    createBox->borderColor = fieldBorder;
    createBox->editingBorderColor = fieldEditingBorder;
    createBox->color = fieldText;
    createBox->placeholderColor = fieldPlaceholder;
}

Rectangle SearchListElement::ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const {
    if (creatingNew) {
        if (childIndex == 2) return ownRect;
        return { ownRect.x, ownRect.y, 0.0f, 0.0f };
    }
    float barH = showSearchBar ? searchBarHeight * uiScale : 0.0f;
    if (childIndex == 0) {
        return { ownRect.x, ownRect.y, ownRect.width, barH };
    }
    if (childIndex == 1) {
        return { ownRect.x, ownRect.y + barH, ownRect.width, ownRect.height - barH };
    }
    return { ownRect.x, ownRect.y, 0.0f, 0.0f };
}

void SearchListElement::EnterCreateMode() {
    creatingNew = true;
    searchBox->visible = false;
    listBox->visible = false;
    createBox->visible = true;
    createBox->text.clear();
    createBox->StartEditing();
}

void SearchListElement::ExitCreateMode() {
    creatingNew = false;
    createBox->visible = false;
    searchBox->visible = showSearchBar;
    listBox->visible = true;
}

void SearchListElement::OnDrawSelf(Rectangle rect, float uiScale) {
    if (backgroundColor.a > 0) DrawRectangleRec(rect, backgroundColor);
}

void SearchListElement::OnTickSelf(float deltaTime, Rectangle rect, float uiScale) {
    if (!creatingNew && searchBox->isEditing && IsKeyPressed(KEY_ENTER) && !highlightedEntryId.empty()) {
        if (onEntrySelected) onEntrySelected(highlightedEntryId);
    }
}

void SearchListElement::RebuildRows() {
    listBox->ClearChildren();

    bool anyExplicitCurrent = false;
    for (const Entry& e : entries) {
        if (e.isCurrent) { anyExplicitCurrent = true; break; }
    }
    highlightedEntryId.clear();

    std::string query = ToLowerCopy(searchBox->text);
    std::string lastCategory = "\x01";

    for (const Entry& entry : entries) {
        if (!query.empty() && ToLowerCopy(entry.label).find(query) == std::string::npos) continue;

        if (!entry.category.empty() && entry.category != lastCategory) {
            auto* header = new TextElement();
            header->text = entry.category;
            header->color = categoryHeaderColor;
            header->fontSize = 11.0f;
            header->anchor = { 0.0f, 0.5f };
            header->pivot = { 0.0f, 0.5f };
            header->offset = { 6.0f, 0.0f };
            header->size = { 0.0f, categoryHeaderHeight };
            listBox->AddChild(header);
            lastCategory = entry.category;
        }

        bool showAsCurrent = entry.isCurrent || (!anyExplicitCurrent && highlightedEntryId.empty() && entry.enabled);
        if (showAsCurrent && highlightedEntryId.empty()) highlightedEntryId = entry.id;

        bool hasReason = !entry.enabled && !entry.disabledReason.empty();
        float thisRowHeight = hasReason ? rowHeight + 14.0f : rowHeight;

        auto* row = new ButtonElement();
        row->size = { 0.0f, thisRowHeight };
        row->defaultColor = showAsCurrent ? rowCurrentColor : Color{ 0, 0, 0, 0 };
        row->hoveredColor = entry.enabled ? rowHoveredColor : Color{ 0, 0, 0, 0 };
        row->selectedColor = entry.enabled ? rowHoveredColor : Color{ 0, 0, 0, 0 };
        row->pressedColor = entry.enabled ? rowHoveredColor : Color{ 0, 0, 0, 0 };
        row->disabled = !entry.enabled;
        if (entry.enabled) {
            row->onActivated = [this, entry]() {
                if (onEntrySelected) onEntrySelected(entry.id);
            };
        }

        auto* label = new TextElement();
        label->text = entry.label;
        label->color = !entry.enabled ? rowDisabledColor : (showAsCurrent ? rowCurrentTextColor : rowColor);
        label->fontSize = 13.0f;
        label->anchor = { 0.0f, 0.0f };
        label->pivot = { 0.0f, 0.0f };
        label->offset = { 8.0f, 2.0f };
        label->size = { 0.0f, 18.0f };
        row->AddChild(label);

        if (hasReason) {
            auto* reasonLabel = new TextElement();
            reasonLabel->text = entry.disabledReason;
            reasonLabel->color = rowDisabledReasonColor;
            reasonLabel->fontSize = 10.0f;
            reasonLabel->anchor = { 0.0f, 0.0f };
            reasonLabel->pivot = { 0.0f, 0.0f };
            reasonLabel->offset = { 8.0f, 18.0f };
            reasonLabel->size = { 0.0f, 14.0f };
            row->AddChild(reasonLabel);
        }

        listBox->AddChild(row);
    }

    if (allowCreate) {
        auto* createRow = new ButtonElement();
        createRow->size = { 0.0f, rowHeight };
        createRow->defaultColor = { 0, 0, 0, 0 };
        createRow->hoveredColor = rowHoveredColor;
        createRow->selectedColor = rowHoveredColor;
        createRow->pressedColor = rowHoveredColor;
        createRow->onActivated = [this]() { EnterCreateMode(); };

        auto* createRowLabel = new TextElement();
        createRowLabel->text = createLabel;
        createRowLabel->color = createRowColor;
        createRowLabel->fontSize = 13.0f;
        createRowLabel->anchor = { 0.0f, 0.5f };
        createRowLabel->pivot = { 0.0f, 0.5f };
        createRowLabel->offset = { 8.0f, 0.0f };
        createRowLabel->size = { 0.0f, 18.0f };
        createRow->AddChild(createRowLabel);

        listBox->AddChild(createRow);
    }
}
