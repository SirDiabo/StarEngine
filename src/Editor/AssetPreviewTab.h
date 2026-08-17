#pragma once
#include "EditorTab.h"
#include "AssetPreviewViewport.h"
#include "ImportDetailsPanel.h"
#include <string>

class AssetPreviewTab : public EditorTab {
public:
    explicit AssetPreviewTab(const std::string& assetPath);
    ~AssetPreviewTab() override;

    void Initialize() override;
    void Render(float x, float y, float width, float height) override;
    void BuildMenus(MenuBar* menuBar) override;

    std::string GetAssetPath() const override { return assetPath; }

private:
    std::string assetPath;
    std::string assetType;
    AssetPreviewViewport viewport;
};
