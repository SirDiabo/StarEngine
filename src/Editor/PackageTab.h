#pragma once
#include "EditorTab.h"
#include "ChoicePopup.h"
#include "ConfirmPopup.h"
#include "Core/PackagePlatform.h"
#include "Core/PackageCipher.h"
#include "Core/PackageRunner.h"
#include "Core/Project.h"
#include "Core/AsyncTask.h"
#include <functional>
#include <string>
#include <vector>

class MenuBar;

class PackageTab : public EditorTab {
public:
    void Initialize() override;
    void Render(float x, float y, float width, float height) override;
    void BuildMenus(MenuBar* menuBar) override;

    void SetUnsavedLevelCheck(std::function<bool()> checkFn) { hasUnsavedLevelChanges = checkFn; }
    void SetSaveLevelCallback(std::function<void()> saveFn) { saveLevelCallback = saveFn; }

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
    ConfirmPopup unsavedLevelPopup;

    std::function<bool()> hasUnsavedLevelChanges;
    std::function<void()> saveLevelCallback;

    bool hasResult = false;
    bool lastSuccess = false;
    std::string resultMessage;
    std::string resultLog;
    float logScrollOffset = 0.0f;

    AsyncTask packageTask;
    bool packageRunning = false;
};
