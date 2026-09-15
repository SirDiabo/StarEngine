#pragma once
#include "Core/Panel.h"
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

class WidgetEditorSession;
class Widget;
class WidgetElement;
class VerticalBoxElement;
class ButtonElement;

class WidgetHierarchyPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~WidgetHierarchyPanel() override;

    void SetSession(WidgetEditorSession* s) { session = s; }
    void SetElementDragCallback(std::function<void(const std::string&, const std::string&)> cb) { dragCallback = cb; }

private:
    struct RowEntry {
        WidgetElement* element = nullptr;
        ButtonElement* rowWidget = nullptr;
    };

    void AddElementRow(VerticalBoxElement* list, WidgetElement* element, int depth);

    WidgetEditorSession* session = nullptr;
    Widget* chrome = nullptr;
    VerticalBoxElement* list = nullptr;
    std::vector<RowEntry> rowEntries;
    std::function<void(const std::string&, const std::string&)> dragCallback;

    bool dragPending = false;
    WidgetElement* dragCandidate = nullptr;
    Vector2 dragStartMouse{ 0.0f, 0.0f };
};
