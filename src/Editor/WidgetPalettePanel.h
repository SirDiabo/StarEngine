#pragma once
#include "Core/Panel.h"

class WidgetEditorSession;
class Widget;

class WidgetPalettePanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~WidgetPalettePanel() override;

    void SetSession(WidgetEditorSession* s) { session = s; }

private:
    void BuildChrome();

    WidgetEditorSession* session = nullptr;
    Widget* chrome = nullptr;
};
