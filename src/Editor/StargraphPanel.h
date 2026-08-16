#pragma once
#include "Core/Panel.h"

class StargraphEditorPanel;

class StargraphPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    void SetSession(StargraphEditorPanel* s) { session = s; }

private:
    StargraphEditorPanel* session = nullptr;
};
