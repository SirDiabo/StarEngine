#pragma once
#include "Core/Panel.h"
#include "Sf2File.h"
#include "raylib.h"
#include <string>
#include <vector>

class SequenceEditorSession;

class Sf2ImportPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    void SetSession(SequenceEditorSession* s) { session = s; }
    void Open();
    bool WantsKeyboard() const { return visible && picking; }

private:
    struct Row {
        int instrument = -1;
        int preset = -1;
        int score = 0;
        bool include = false;
    };

    void LoadSoundFont(const std::string& path);
    void ResetDestination();
    void AutoMatchAll();
    void SyncRows();
    void RunImport();
    void DrawPicker(const Rectangle& area);
    std::string DefaultDestination() const;
    std::string InstrumentSignature() const;
    std::string PresetLabel(int preset) const;

    SequenceEditorSession* session = nullptr;
    Starsequence::Sf2::File soundFont;
    bool loaded = false;
    std::string soundFontPath;
    std::string destination;
    bool destinationCustom = false;
    std::string status;
    bool statusIsError = false;

    std::vector<Row> rows;
    std::string signature;
    float scroll = 0.0f;

    bool picking = false;
    int pickRow = -1;
    std::string filter;
    float pickScroll = 0.0f;
    std::vector<int> sortedPresets;
};
