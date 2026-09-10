#pragma once
#include "Core/Panel.h"
#include <vector>
#include <functional>
#include <string>

class WidgetEditorSession;
class WidgetElement;
class Widget;
class VerticalBoxElement;
class TextInputElement;

class WidgetDetailsPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~WidgetDetailsPanel() override;

    void SetSession(WidgetEditorSession* s) { session = s; }

private:
    struct FieldRow {
        TextInputElement* input = nullptr;
        std::function<std::string(WidgetElement&)> getValue;
        std::function<void(WidgetElement&, const std::string&)> setValue;
    };

    void RebuildForSelection(WidgetElement* element);
    void AddStringField(VerticalBoxElement* list, const std::string& label,
        std::function<std::string(WidgetElement&)> getValue,
        std::function<void(WidgetElement&, const std::string&)> setValue);
    void AddBoolField(VerticalBoxElement* list, const std::string& label,
        std::function<bool(WidgetElement&)> getValue,
        std::function<void(WidgetElement&, bool)> setValue);

    WidgetEditorSession* session = nullptr;
    Widget* chrome = nullptr;
    WidgetElement* builtForElement = nullptr;
    std::vector<FieldRow> fieldRows;
};
