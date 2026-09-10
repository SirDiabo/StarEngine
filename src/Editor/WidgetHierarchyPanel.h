#pragma once
#include "Core/Panel.h"

class WidgetEditorSession;
class Widget;
class WidgetElement;
class VerticalBoxElement;

class WidgetHierarchyPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~WidgetHierarchyPanel() override;

    void SetSession(WidgetEditorSession* s) { session = s; }

private:
    void AddElementRow(VerticalBoxElement* list, WidgetElement* element, int depth);

    WidgetEditorSession* session = nullptr;
    Widget* chrome = nullptr;
};
