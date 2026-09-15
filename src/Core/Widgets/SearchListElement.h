#pragma once
#include "WidgetElement.h"
#include <functional>
#include <string>
#include <vector>

class TextElement;
class BoxElement;

class SearchListElement : public WidgetElement {
public:
    struct Entry {
        std::string id;
        std::string label;
        std::string category;
        bool enabled = true;
        std::string disabledReason;
    };

    SearchListElement();

    const char* GetClassName() const override { return "SearchListElement"; }
    std::string GetObjectKind() const override { return "SearchListElement"; }

    bool showSearchBar = true;
    float searchBarHeight = 26.0f;
    float rowHeight = 22.0f;
    float categoryHeaderHeight = 18.0f;

    Color backgroundColor{ 30, 30, 34, 255 };
    Color categoryHeaderColor{ 150, 150, 155, 255 };
    Color rowColor{ 220, 220, 225, 255 };
    Color rowHoveredColor{ 55, 55, 60, 255 };
    Color rowDisabledColor{ 110, 110, 115, 255 };
    Color rowDisabledReasonColor{ 130, 130, 135, 255 };

    std::function<void(const std::string&)> onEntrySelected;

    bool allowCreate = false;
    std::string createLabel = "+ New...";
    Color createRowColor{ 120, 195, 120, 255 };
    std::function<void(const std::string&)> onCreateRequested;

    void SetEntries(std::vector<Entry> newEntries);
    void SetShowSearchBar(bool show);

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
    Rectangle ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const override;

private:
    void RebuildRows();
    void EnterCreateMode();
    void ExitCreateMode();

    std::vector<Entry> entries;
    TextElement* searchBox = nullptr;
    BoxElement* listBox = nullptr;
    TextElement* createBox = nullptr;
    bool creatingNew = false;
};
