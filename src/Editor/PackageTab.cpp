#include "PackageTab.h"
#include "MenuBar.h"
#include "NativeFileDialog.h"
#include "Core/PackageRunner.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>

namespace fs = std::filesystem;

void PackageTab::Initialize() {
    title     = "Package Project";
    permanent = false;

    platforms = GetAvailablePackagePlatforms();
    for (int i = 0; i < (int)platforms.size(); i++) {
        if (platforms[i].available) { selectedPlatformIndex = i; break; }
    }

    renderer = Project::Get().GetSettings().renderer;
    outputDir = (fs::path(Project::Get().GetRootPath()) / "Packaged").string();
}

static Rectangle SelectRow(float x, float y, float w, float h) {
    return {x, y, w, h};
}

void PackageTab::Render(float x, float y, float width, float height) {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel({x, y, width, height});

    Vector2 mp = GetMousePosition();
    float fx = x + 16;
    float fy = y + 16;
    float fw = width - 32 > 640.0f ? 640.0f : width - 32;
    const bool anyPopupOpen = platformPopup.IsOpen() || rendererPopup.IsOpen() ||
                               cipherPopup.IsOpen() || formatPopup.IsOpen();

    DrawText("Package Project", (int)fx, (int)fy, 14, theme.sectionHeader);
    fy += 26;

    // Platform
    DrawText("Platform", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle platformRow = SelectRow(fx, fy, fw, 22);
    DrawRectangleRec(platformRow, theme.fieldBg);
    DrawRectangleLinesEx(platformRow, 1, theme.fieldBorder);
    const PackagePlatformInfo& selectedPlatform = platforms[selectedPlatformIndex];
    DrawText(selectedPlatform.displayName.c_str(), (int)platformRow.x + 6, (int)platformRow.y + 5, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, platformRow) && !anyPopupOpen) {
        std::vector<ChoiceOption> options;
        for (const auto& p : platforms) options.push_back({p.displayName, p.available, p.unavailableReason});
        platformPopup.Open({platformRow.x, platformRow.y + platformRow.height}, fw, options);
    }
    fy += 30;

    // Renderer
    DrawText("Renderer", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle rendererRow = SelectRow(fx, fy, fw, 22);
    DrawRectangleRec(rendererRow, theme.fieldBg);
    DrawRectangleLinesEx(rendererRow, 1, theme.fieldBorder);
    std::string rendererLabel = renderer == GameRenderer::Software
        ? "Software (CPU, no GPU required)" : "Hardware (OpenGL)";
    DrawText(rendererLabel.c_str(), (int)rendererRow.x + 6, (int)rendererRow.y + 5, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, rendererRow) && !anyPopupOpen) {
        rendererPopup.Open({rendererRow.x, rendererRow.y + rendererRow.height}, fw, {
            {"Hardware (OpenGL)", true, ""},
            {"Software (CPU, no GPU required)", true, ""},
        });
    }
    fy += 20;
    DrawText("Saved with the project. Only changes how packaged games render; the editor always uses Hardware.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;

    // Encryption
    DrawText("Content Encryption", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle cipherRow = SelectRow(fx, fy, fw, 22);
    DrawRectangleRec(cipherRow, theme.fieldBg);
    DrawRectangleLinesEx(cipherRow, 1, theme.fieldBorder);
    DrawText(CookCipherDisplayName(cipher).c_str(), (int)cipherRow.x + 6, (int)cipherRow.y + 5, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, cipherRow) && !anyPopupOpen) {
        std::vector<ChoiceOption> options;
        for (CookCipher c : AllCookCiphers()) options.push_back({CookCipherDisplayName(c), true, ""});
        cipherPopup.Open({cipherRow.x, cipherRow.y + cipherRow.height}, fw, options);
    }
    fy += 20;
    DrawText("A fresh key is generated for every package. XOR deters casual inspection, it is not secure.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;

    // Format
    DrawText("Packaging Format", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle formatRow = SelectRow(fx, fy, fw, 22);
    DrawRectangleRec(formatRow, theme.fieldBg);
    DrawRectangleLinesEx(formatRow, 1, theme.fieldBorder);
    std::string formatLabel = format == PackageFormat::LooseFolder
        ? "Loose Content folder (debug)" : "Single file (.starpak embedded)";
    DrawText(formatLabel.c_str(), (int)formatRow.x + 6, (int)formatRow.y + 5, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, formatRow) && !anyPopupOpen) {
        formatPopup.Open({formatRow.x, formatRow.y + formatRow.height}, fw, {
            {"Single file (.starpak embedded)", true, ""},
            {"Loose Content folder (debug)", true, ""},
        });
    }
    fy += 20;
    DrawText("Single file ships one exe with no loose Content/ folder. Loose folder is faster to re-package for testing.",
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;

    // Output folder
    DrawText("Output Folder", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    float browseWidth = 70.0f;
    Rectangle outputRow = SelectRow(fx, fy, fw - browseWidth - 6, 22);
    DrawRectangleRec(outputRow, theme.fieldBg);
    DrawRectangleLinesEx(outputRow, 1, theme.fieldBorder);
    DrawText(outputDir.c_str(), (int)outputRow.x + 6, (int)outputRow.y + 5, 11, theme.textPrimary);
    Rectangle browseButton = {outputRow.x + outputRow.width + 6, fy, browseWidth, 22};
    if (UIWidget::Button(browseButton, "Browse...") && !anyPopupOpen) {
        std::string chosen = NativeFileDialog::OpenFolder("Choose Output Folder", outputDir);
        if (!chosen.empty()) outputDir = chosen;
    }
    fy += 34;

    // Package button
    Rectangle packageButton = {fx, fy, fw, 28};
    bool platformReady = selectedPlatform.available;
    if (UIWidget::Button(packageButton, "Package", platformReady) && !anyPopupOpen) {
        RunPackage();
    }
    fy += 36;

    if (!platformReady) {
        DrawText(("Cannot package for this platform: " + selectedPlatform.unavailableReason).c_str(),
                  (int)fx, (int)fy, 10, theme.danger);
        fy += 18;
    }

    if (hasResult) {
        Color resultColor = lastSuccess ? theme.success : theme.danger;
        DrawText(resultMessage.c_str(), (int)fx, (int)fy, 12, resultColor);
        fy += 22;

        Rectangle copyButton = {fx, fy, 100.0f, 20.0f};
        if (UIWidget::Button(copyButton, "Copy Log") && !anyPopupOpen) {
            SetClipboardText(resultLog.c_str());
        }
        fy += 26;

        float logHeight = (y + height) - fy - 12.0f;
        if (logHeight < 100.0f) logHeight = 100.0f;
        float logWidth = width - 32.0f;
        Rectangle logBox = {fx, fy, logWidth, logHeight};
        UIWidget::ScrollableTextBox(logBox, resultLog, logScrollOffset);
    }

    int platformChoice = 0;
    if (platformPopup.Render(platformChoice)) selectedPlatformIndex = platformChoice;

    int rendererChoice = 0;
    if (rendererPopup.Render(rendererChoice)) {
        renderer = rendererChoice == 1 ? GameRenderer::Software : GameRenderer::Hardware;
        Project::Get().GetSettings().renderer = renderer;
    }

    int cipherChoice = 0;
    if (cipherPopup.Render(cipherChoice)) {
        cipher = AllCookCiphers()[cipherChoice];
    }

    int formatChoice = 0;
    if (formatPopup.Render(formatChoice)) {
        format = formatChoice == 1 ? PackageFormat::LooseFolder : PackageFormat::SingleFile;
    }
}

void PackageTab::RunPackage() {
    PackageOptions options;
    options.projectRoot = Project::Get().GetRootPath();
    options.outputDir = outputDir;
    options.platform = platforms[selectedPlatformIndex].platform;
    options.renderer = renderer;
    options.cipher = cipher;
    options.format = format;

    PackageResult result = PackageRunner::Package(options);

    hasResult = true;
    lastSuccess = result.success;
    resultLog = result.log;
    resultMessage = result.success
        ? "Packaged successfully: " + result.outputExecutable
        : "Packaging failed: " + result.error;
}

void PackageTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { "Save Project" } },
        { "Edit",   { } },
        { "Window", { } },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}