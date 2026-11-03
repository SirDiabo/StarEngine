#pragma once
#include "Core/Panel.h"
#include "Editor/AssetPickerPopup.h"
#include "Modules/Starsequence/SequenceData.h"
#include "raylib.h"
#include <string>

class SequenceEditorSession;

class InstrumentEditorPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    void SetSession(SequenceEditorSession* s) { session = s; }
    void Open();
    bool WantsKeyboard() const { return visible && !activeField.empty(); }

private:
    bool NumberField(const std::string& id, Rectangle r, float& value, int decimals, float lo, float hi, bool integer);
    bool TextField(const std::string& id, Rectangle r, std::string& value, size_t maxLength);
    bool VSlider(const std::string& id, Rectangle r, float& value, float lo, float hi, float power, float resetValue);
    bool Hot(Rectangle r) const;
    void DrawEnvelope(Rectangle r, const Starsequence::SampleSet& set) const;
    float DrawKeyMap(Rectangle r, Starsequence::SampleSet& set, int instrument, int setIndex);

    SequenceEditorSession* session = nullptr;
    AssetPickerPopup picker;
    int pickerZone = -1;
    int selectedZone = -1;

    std::string activeField;
    std::string buffer;
    bool replaceOnType = false;
    std::string pendingField;
    std::string pendingBuffer;

    std::string activeSlider;
    std::string lastSliderId;
    double lastSliderClick = -10.0;

    float scroll = 0.0f;
    float contentHeight = 0.0f;
};
