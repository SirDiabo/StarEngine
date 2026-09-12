#pragma once
#include "Core/Widgets/WidgetAsset.h"
#include <string>
#include <functional>

class WidgetElement;

class WidgetEditorSession {
public:
    bool Open(const std::string& path);
    bool Save();
    ~WidgetEditorSession();

    bool IsOpen() const { return isOpen; }
    const std::string& GetAssetPath() const { return assetPath; }

    WidgetAssetData& GetData() { return data; }
    const WidgetAssetData& GetData() const { return data; }

    WidgetElement* GetSelectedElement() const { return lastValidElement; }
    void SetSelectedElement(WidgetElement* element) {
        highlightedElement = element;
        lastValidElement = element;
    }

    bool IsHierarchyRowHighlighted(WidgetElement* element) const { return highlightedElement == element; }
    void ClearHierarchyHighlight() { highlightedElement = nullptr; }

    WidgetElement* AddElement(const std::string& className, WidgetElement* parent);
    void DeleteElement(WidgetElement* element);

    bool IsDirty() const { return dirty; }
    void MarkDirty() { dirty = true; }

    std::function<void()> beforeSave;

private:
    std::string assetPath;
    WidgetAssetData data;
    WidgetElement* highlightedElement = nullptr;
    WidgetElement* lastValidElement = nullptr;
    bool dirty = false;
    bool isOpen = false;
};
