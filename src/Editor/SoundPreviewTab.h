#pragma once
#include "EditorTab.h"
#include "Runtime/Audio/AudioSource.h"
#include <memory>
#include <string>
#include <vector>

class SoundPreviewTab : public EditorTab {
public:
    explicit SoundPreviewTab(const std::string& assetPath);
    ~SoundPreviewTab() override;

    void Initialize() override;
    void Render(float x, float y, float width, float height) override;
    void BuildMenus(MenuBar* menuBar) override;
    void OnDeactivated() override;

    std::string GetAssetPath() const override { return assetPath; }

private:
    void BuildWaveform();
    void StartPlayback();
    void StopPlayback();
    bool IsPlaying() const;

    std::string assetPath;
    std::unique_ptr<AudioSource> source;
    std::vector<float> peakMin;
    std::vector<float> peakMax;
    std::string formatText;
    double duration = 0.0;
    bool looping = false;
    bool draggingPlayhead = false;
    int appliedRevision = 0;
};
