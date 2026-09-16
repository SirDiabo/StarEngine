#pragma once
#include "Core/Panel.h"
#include "raylib.h"
#include <functional>
#include <string>

class WidgetEditorSession;
class Widget;
class WidgetElement;
class TreeListElement;

class WidgetHierarchyPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~WidgetHierarchyPanel() override;

    void SetSession(WidgetEditorSession* s) { session = s; }
    void SetElementDragCallback(std::function<void(const std::string&, const std::string&)> cb) { dragCallback = cb; }

private:
    WidgetEditorSession* session = nullptr;
    Widget* chrome = nullptr;
    TreeListElement* list = nullptr;
    std::function<void(const std::string&, const std::string&)> dragCallback;
};
