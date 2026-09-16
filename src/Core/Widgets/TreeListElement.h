#pragma once
#include "BoxElement.h"
#include <functional>
#include <string>
#include <vector>

class TextElement;
class ButtonElement;

enum class TreeDropPosition {
    Before,
    After,
    Into
};

class TreeListElement : public BoxElement {
public:
    struct Entry {
        std::string id;
        std::string label;
        std::string parentId;
        Color iconColor{ 0, 0, 0, 0 };
        bool isCurrent = false;
    };

    TreeListElement();

    const char* GetClassName() const override { return "TreeListElement"; }
    std::string GetObjectKind() const override { return "TreeListElement"; }

    std::string dragKind = "TreeRow";
    bool allowReorder = true;
    bool allowReparent = false;
    bool allowExternalDrag = true;
    float rowHeight = 20.0f;
    float indentWidth = 16.0f;

    Color rowColor{ 210, 210, 215, 255 };
    Color rowAltColor{ 0, 0, 0, 0 };
    Color rowHoveredColor{ 55, 55, 60, 255 };
    Color rowCurrentColor{ 60, 80, 110, 255 };
    Color rowCurrentTextColor{ 235, 235, 240, 255 };
    Color dropLineColor{ 90, 140, 220, 255 };
    Color dropIntoColor{ 70, 100, 70, 180 };

    std::function<void(const std::string&)> onEntrySelected;
    std::function<void(const std::string& draggedId, const std::string& targetId, TreeDropPosition pos)> onReorderOrReparent;
    std::function<void(const std::string& draggedId)> onExternalDragStarted;
    std::function<void(const std::string&)> onDeleteRequested;

    void SetEntries(std::vector<Entry> newEntries);

    void Draw(Rectangle rect, float uiScale) override;

protected:
    void OnTickSelf(float deltaTime, Rectangle rect, float uiScale) override;

private:
    void RebuildRows();
    void AddRowRecursive(int entryIndex, int depth, int& visualIndex);

    struct RowInfo {
        std::string id;
    };

    std::vector<Entry> entries;
    std::vector<RowInfo> rowInfos;
    bool externalDragFired = false;
};
