#include "WidgetEditorSession.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/WidgetElementFields.h"
#include "Core/Widgets/WidgetAssetCache.h"
#include "Core/Widgets/WidgetObjectElement.h"
#include "Core/ProgrammingInterfaceRegistry.h"

bool WidgetEditorSession::Open(const std::string& path) {
    WidgetObjectElement::BehaviorScope inert(false);
    if (!WidgetAsset::LoadWidgetAsset(path, data)) return false;
    assetPath = path;
    isOpen = true;
    dirty = false;
    highlightedElement = nullptr;
    lastValidElement = nullptr;

    if (data.programmingInterface.empty()) {
        std::vector<const ProgrammingInterfaceInfo*> providers = ProgrammingInterfaceRegistry::Get().GetAvailableForWidgets();
        if (providers.size() == 1) {
            data.programmingInterface = providers[0]->name;
            if (providers[0]->createDefaultContent) data.behaviorContent = providers[0]->createDefaultContent();
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

std::string WidgetEditorSession::CaptureDesign() const {
    if (!isOpen) return std::string();
    return WidgetAsset::SerializeWidgetAsset(data, false);
}

std::string WidgetEditorSession::SelectedElementId() const {
    return lastValidElement ? lastValidElement->elementId : std::string();
}

static WidgetElement* FindElementById(WidgetElement* root, const std::string& id) {
    if (!root) return nullptr;
    if (root->elementId == id) return root;
    for (WidgetElement* child : root->GetAuthoredChildren()) {
        if (WidgetElement* found = FindElementById(child, id)) return found;
    }
    return nullptr;
}

void WidgetEditorSession::RestoreDesign(const std::string& text, const std::string& selectedId) {
    if (!isOpen || text.empty()) return;

    WidgetObjectElement::BehaviorScope inert(false);
    WidgetAssetData restored;
    if (!WidgetAsset::DeserializeWidgetAsset(text, restored, false)) return;

    highlightedElement = nullptr;
    lastValidElement = nullptr;

    delete data.rootElement;
    data.rootElement = restored.rootElement;
    restored.rootElement = nullptr;
    data.assetName = restored.assetName;
    data.parentClass = restored.parentClass;
    data.uiScale = restored.uiScale;
    data.forceNativeResolution = restored.forceNativeResolution;

    if (!selectedId.empty()) {
        WidgetElement* element = FindElementById(data.rootElement, selectedId);
        highlightedElement = element;
        lastValidElement = element;
    }
    dirty = true;
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
        const std::vector<WidgetElement*>& siblings = newParent->GetAuthoredChildren();
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
