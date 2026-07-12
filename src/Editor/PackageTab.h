#pragma once
#include "EditorTab.h"
#include "ChoicePopup.h"
#include "Core/PackagePlatform.h"
#include "Core/PackageCipher.h"
#include "Core/PackageRunner.h"
#include "Core/Project.h"
#include <string>
#include <vector>

class MenuBar;

class PackageTab : public EditorTab {
public:
    void Initialize() override;
    void Render(float x, float y, float width, float height) override;
    void BuildMenus(MenuBar* menuBar) override;

private:
    void RunPackage();

    std::vector<PackagePlatformInfo> platforms;
    int selectedPlatformIndex = 0;
    GameRenderer renderer = GameRenderer::Hardware;
    CookCipher cipher = CookCipher::None;
    PackageFormat format = PackageFormat::SingleFile;
    std::string outputDir;

    ChoicePopup platformPopup;
    ChoicePopup rendererPopup;
    ChoicePopup cipherPopup;
    
    ChoicePopup formatPopup;

    bool hasResult = false;
    bool lastSuccess = false;
    std::string resultMessage;
    std::string resultLog;
};
