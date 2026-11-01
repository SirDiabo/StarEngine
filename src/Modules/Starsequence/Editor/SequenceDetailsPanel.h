#pragma once
#include "Editor/DetailsPanel.h"
#include <string>
#include <vector>

class SequenceEditorSession;

class SequenceDetailsPanel : public DetailsPanel {
public:
    void SetSession(SequenceEditorSession* s);

private:
    std::vector<PropertyRow> BuildRows();
    void OnAssetPickedForRow(const std::string& fullRowId, const std::string& pickedPath);

    SequenceEditorSession* session = nullptr;
};
