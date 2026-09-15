#include "SearchableListPopup.h"
#include "Core/Widgets/WidgetOverlay.h"
#include "Core/Widgets/SearchListElement.h"

void SearchableListPopup::Open(Vector2 anchorPos, float w, float maxH) {
    open = true;
    hasPending = false;
    pos = anchorPos;
    width = w;
    maxVisibleHeight = maxH;
    lastRect = { pos.x, pos.y, width, maxVisibleHeight };

    auto* list = new SearchListElement();
    list->onEntrySelected = [this](const std::string& id) {
        pendingId = id;
        hasPending = true;
    };
    content = list;

    WidgetOverlay::Open(list, pos, { width, maxVisibleHeight }, [this]() {
        open = false;
        content = nullptr;
    });
}

void SearchableListPopup::Close() {
    open = false;
    content = nullptr;
    WidgetOverlay::Close();
}

std::string SearchableListPopup::Render(const std::vector<Entry>& entries, Rectangle boundsArea) {
    if (!open || !content) return "";

    std::vector<SearchListElement::Entry> listEntries;
    listEntries.reserve(entries.size());
    for (const Entry& e : entries) {
        SearchListElement::Entry le;
        le.category = e.category;
        le.id = e.id;
        le.label = e.label;
        listEntries.push_back(le);
    }
    content->SetEntries(listEntries);

    if (hasPending) {
        std::string result = pendingId;
        hasPending = false;
        Close();
        return result;
    }
    return "";
}

void SearchableListPopup::FlushDeferredDraws() {
}
