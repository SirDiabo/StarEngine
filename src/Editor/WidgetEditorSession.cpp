#include "WidgetEditorSession.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/WidgetElementFields.h"
#include "Core/Widgets/WidgetAssetCache.h"
#include "Core/ProgrammingInterfaceRegistry.h"

bool WidgetEditorSession::Open(const std::string& path) {
    if (!WidgetAsset::LoadWidgetAsset(path, data)) return false;
    assetPath = path;
    isOpen = true;
    dirty = false;
    highlightedElement = nullptr;
    lastValidElement = nullptr;

    if (data.programmingInterface.empty()) {
        const std::vector<ProgrammingInterfaceInfo>& providers = ProgrammingInterfaceRegistry::Get().GetAll();
        if (providers.size() == 1) {
            data.programmingInterface = providers[0].name;
            if (providers[0].createDefaultContent) data.behaviorContent = providers[0].createDefaultContent();
            dirty = true;
        }
    }

    return true;
}

bool WidgetEditorSession::Save() {
    if (!isOpen) return false;
    if (beforeSave) beforeSave();
    if (!WidgetAsset::SaveWidgetAsset(assetPath, data)) return false;
    WidgetAssetCache::Get().Invalidate(assetPath);
    dirty = false;
    return true;
}

WidgetEditorSession::~WidgetEditorSession() {
    delete data.rootElement;
    delete data.behaviorContent;
}

WidgetElement* WidgetEditorSession::AddElement(const std::string& className, WidgetElement* parent) {
    WidgetElement* element = CreateDefaultWidgetElement(className);
    if (!element) return nullptr;

    if (!parent) parent = data.rootElement;
    if (parent) {
        parent->AddChild(element);
    } else {
        data.rootElement = element;
    }

    dirty = true;
    highlightedElement = element;
    lastValidElement = element;
    return element;
}

void WidgetEditorSession::DeleteElement(WidgetElement* element) {
    if (!element || element == data.rootElement) return;
    if (highlightedElement == element) highlightedElement = nullptr;
    if (lastValidElement == element) lastValidElement = nullptr;
    if (element->parent) element->parent->RemoveChild(element);
    dirty = true;
}

void WidgetEditorSession::ReparentElement(WidgetElement* element, WidgetElement* targetElement, TreeDropPosition pos) {
    if (!element || !targetElement || element == data.rootElement || element == targetElement) return;

    WidgetElement* newParent = nullptr;
    int insertIndex = -1;

    if (pos == TreeDropPosition::Into) {
        newParent = targetElement;
    } else {
        newParent = targetElement->parent;
        if (!newParent) return;
        const std::vector<WidgetElement*>& siblings = newParent->GetChildren();
        for (int i = 0; i < (int)siblings.size(); i++) {
            if (siblings[i] == targetElement) {
                insertIndex = (pos == TreeDropPosition::Before) ? i : i + 1;
                break;
            }
        }
    }
    if (!newParent) return;

    WidgetElement* check = newParent;
    while (check) {
        if (check == element) return;
        check = check->parent;
    }

    if (element->parent) element->parent->DetachChild(element);
    if (insertIndex >= 0) newParent->InsertChild(element, insertIndex);
    else newParent->AddChild(element);
    dirty = true;
}
