#pragma once
#include "EditorTab.h"
#include "AssetPreviewViewport.h"
#include "SearchableListPopup.h"
#include <string>
#include <vector>

class AnimationPreviewTab : public EditorTab {
public:
    explicit AnimationPreviewTab(const std::string& animPath);

    void Initialize() override;
    void Render(float x, float y, float width, float height) override;
    void BuildMenus(MenuBar* menuBar) override;

    const std::string& GetAssetPath() const { return animPath; }

private:
    void RefreshCompatibleMeshes();

    std::string animPath;
    std::string meshPath;

    AssetPreviewViewport viewport;
    SearchableListPopup meshPicker;
    std::vector<std::string> compatibleMeshPaths;

    bool playing = false;
    float timeSeconds = 0.0f;
    float fps = 30.0f;
    int frameCount = 0;
    unsigned int animBoneCount = 0;
    std::string animName;
};
