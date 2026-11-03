#pragma once
#include "Core/Panel.h"
#include "Editor/AssetPickerPopup.h"
#include "Modules/Starsequence/SequenceData.h"
#include "raylib.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

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

    struct ZoneWave {
        std::vector<float> peakMin;
        std::vector<float> peakMax;
        uint64_t frames = 0;
        bool ok = false;
        bool hasLoop = false;
        uint32_t wavLoopStart = 0;
        uint32_t wavLoopEnd = 0;
        std::filesystem::file_time_type stamp{};
        double checkedAt = 0.0;
    };
    const ZoneWave* GetWave(const std::string& assetPath);
    bool DrawZoneWave(Rectangle r, Starsequence::SampleZone& z, int zoneIndex, const std::string& id, int instrument, int setIndex);
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

    std::unordered_map<std::string, ZoneWave> waveCache;
    int wavesBuiltThisFrame = 0;
    std::string loopDragId;
    int loopDragWhich = 0;
    bool loopDragChanged = false;
    bool loopPreviewHeld = false;

    float scroll = 0.0f;
    float contentHeight = 0.0f;
};
