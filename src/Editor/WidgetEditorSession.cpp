#include "WidgetEditorSession.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/WidgetElementFields.h"
#include "Core/ProgrammingInterfaceRegistry.h"

bool WidgetEditorSession::Open(const std::string& path) {
    if (!WidgetAsset::LoadWidgetAsset(path, data)) return false;
    assetPath = path;
    isOpen = true;
    dirty = false;
    selectedElement = nullptr;

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
    selectedElement = element;
    return element;
}

void WidgetEditorSession::DeleteElement(WidgetElement* element) {
    if (!element || element == data.rootElement) return;
    if (selectedElement == element) selectedElement = nullptr;
    if (element->parent) element->parent->RemoveChild(element);
    dirty = true;
}
