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

    WidgetElement* GetSelectedElement() const { return selectedElement; }
    void SetSelectedElement(WidgetElement* element) { selectedElement = element; }

    WidgetElement* AddElement(const std::string& className, WidgetElement* parent);
    void DeleteElement(WidgetElement* element);

    bool IsDirty() const { return dirty; }
    void MarkDirty() { dirty = true; }

    std::function<void()> beforeSave;

private:
    std::string assetPath;
    WidgetAssetData data;
    WidgetElement* selectedElement = nullptr;
    bool dirty = false;
    bool isOpen = false;
};
