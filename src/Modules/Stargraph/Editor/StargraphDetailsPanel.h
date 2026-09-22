#pragma once
#include "Editor/DetailsPanel.h"

class StargraphEditorPanel;

class StargraphDetailsPanel : public DetailsPanel {
public:
    void SetSession(StargraphEditorPanel* s);

private:
    StargraphEditorPanel* session = nullptr;
};
