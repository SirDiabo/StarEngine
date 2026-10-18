#include "WidgetHierarchyPanel.h"
#include "WidgetEditorSession.h"
#include "Editor/UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/TreeListElement.h"
#include "raylib.h"

namespace {

void FlattenEntries(WidgetElement* element, const std::string& parentId, WidgetEditorSession* session,
                     std::vector<TreeListElement::Entry>& out) {
    TreeListElement::Entry entry;
    entry.id = element->elementId;
    entry.label = element->elementId + "  (" + element->GetClassName() + ")";
    entry.parentId = parentId;
    entry.isCurrent = session && session->GetSelectedElement() == element;
    out.push_back(entry);

    for (WidgetElement* child : element->GetAuthoredChildren()) {
        FlattenEntries(child, element->elementId, session, out);
    }
}

WidgetElement* FindById(WidgetElement* root, const std::string& id) {
    if (!root) return nullptr;
    if (root->elementId == id) return root;
    for (WidgetElement* child : root->GetAuthoredChildren()) {
        if (WidgetElement* found = FindById(child, id)) return found;
    }
    return nullptr;
}

}

void WidgetHierarchyPanel::Initialize() {
    name = "Hierarchy";
}

WidgetHierarchyPanel::~WidgetHierarchyPanel() {
    delete chrome;
}

void WidgetHierarchyPanel::Render() {
    if (!chrome) {
        chrome = new Widget();
        list = new TreeListElement();
        list->dragKind = "WidgetElement";
        list->allowReorder = true;
        list->allowReparent = true;
        list->allowExternalDrag = true;

        list->onEntrySelected = [this](const std::string& id) {
            if (!session) return;
            WidgetElement* element = FindById(session->GetData().rootElement, id);
            session->SetSelectedElement(element);
        };
        list->onReorderOrReparent = [this](const std::string& draggedId, const std::string& targetId, TreeDropPosition pos) {
            if (!session) return;
            WidgetElement* dragged = FindById(session->GetData().rootElement, draggedId);
            WidgetElement* target = FindById(session->GetData().rootElement, targetId);
            session->ReparentElement(dragged, target, pos);
        };
        list->onExternalDragStarted = [this](const std::string& draggedId) {
            if (!session) return;
            WidgetElement* element = FindById(session->GetData().rootElement, draggedId);
            if (element && dragCallback) dragCallback(element->elementId, element->GetClassName());
        };
        list->onDeleteRequested = [this](const std::string& id) {
            if (!session) return;
            WidgetElement* element = FindById(session->GetData().rootElement, id);
            if (element) session->DeleteElement(element);
        };

        chrome->SetRoot(list);
    }

    const UIPalette& theme = UITheme::Get();
    list->backgroundColor = theme.panelBg;
    list->rowColor = theme.textPrimary;
    list->rowAltColor = { 0, 0, 0, 20 };
    list->rowHoveredColor = theme.surfaceHover;
    list->rowCurrentColor = theme.surfaceSelected;
    list->rowCurrentTextColor = theme.textPrimary;
    list->dropLineColor = theme.accent;
    list->size = { bounds.width, bounds.height };

    std::vector<TreeListElement::Entry> entries;
    if (session && session->GetData().rootElement) {
        FlattenEntries(session->GetData().rootElement, "", session, entries);
    }
    list->SetEntries(entries);

    float dt = GetFrameTime();
    chrome->Tick(dt, bounds, 1.0f);
    Vector2 mouse = GetMousePosition();
    WidgetInput::ProcessInput(chrome, "Editor", dt, mouse, 1.0f);
    chrome->Draw(bounds, 1.0f);
}
