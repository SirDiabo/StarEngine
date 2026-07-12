#include "PackageTab.h"
#include "MenuBar.h"
#include "NativeFileDialog.h"
#include "Core/PackageRunner.h"
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
    DrawRectangleRec({x, y, width, height}, {32, 32, 35, 255});

    Vector2 mp = GetMousePosition();
    float fx = x + 16;
    float fy = y + 16;
    float fw = width - 32 > 460.0f ? 460.0f : width - 32;

    DrawText("Package Project", (int)fx, (int)fy, 14, {180, 200, 230, 255});
    fy += 26;

    // Platform
    DrawText("Platform", (int)fx, (int)fy, 11, {140, 140, 150, 255});
    fy += 14;
    Rectangle platformRow = SelectRow(fx, fy, fw, 22);
    DrawRectangleRec(platformRow, {26, 26, 28, 255});
    DrawRectangleLinesEx(platformRow, 1, {55, 55, 60, 255});
    const PackagePlatformInfo& selectedPlatform = platforms[selectedPlatformIndex];
    DrawText(selectedPlatform.displayName.c_str(), (int)platformRow.x + 6, (int)platformRow.y + 5, 12, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, platformRow) && !platformPopup.IsOpen()) {
        std::vector<ChoiceOption> options;
        for (const auto& p : platforms) options.push_back({p.displayName, p.available, p.unavailableReason});
        platformPopup.Open({platformRow.x, platformRow.y + platformRow.height}, fw, options);
    }
    fy += 30;

    // Renderer
    DrawText("Renderer", (int)fx, (int)fy, 11, {140, 140, 150, 255});
    fy += 14;
    Rectangle rendererRow = SelectRow(fx, fy, fw, 22);
    DrawRectangleRec(rendererRow, {26, 26, 28, 255});
    DrawRectangleLinesEx(rendererRow, 1, {55, 55, 60, 255});
    std::string rendererLabel = renderer == GameRenderer::Software
        ? "Software (CPU, no GPU required)" : "Hardware (OpenGL)";
    DrawText(rendererLabel.c_str(), (int)rendererRow.x + 6, (int)rendererRow.y + 5, 12, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, rendererRow) && !rendererPopup.IsOpen()) {
        rendererPopup.Open({rendererRow.x, rendererRow.y + rendererRow.height}, fw, {
            {"Hardware (OpenGL)", true, ""},
            {"Software (CPU, no GPU required)", true, ""},
        });
    }
    fy += 20;
    DrawText("Saved with the project. Only changes how packaged games render; the editor always uses Hardware.",
              (int)fx, (int)fy, 10, {110, 110, 118, 255});
    fy += 24;

    // Encryption
    DrawText("Content Encryption", (int)fx, (int)fy, 11, {140, 140, 150, 255});
    fy += 14;
    Rectangle cipherRow = SelectRow(fx, fy, fw, 22);
    DrawRectangleRec(cipherRow, {26, 26, 28, 255});
    DrawRectangleLinesEx(cipherRow, 1, {55, 55, 60, 255});
    DrawText(CookCipherDisplayName(cipher).c_str(), (int)cipherRow.x + 6, (int)cipherRow.y + 5, 12, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, cipherRow) && !cipherPopup.IsOpen()) {
        std::vector<ChoiceOption> options;
        for (CookCipher c : AllCookCiphers()) options.push_back({CookCipherDisplayName(c), true, ""});
        cipherPopup.Open({cipherRow.x, cipherRow.y + cipherRow.height}, fw, options);
    }
    fy += 20;
    DrawText("A fresh key is generated for every package. XOR deters casual inspection, it is not secure.",
              (int)fx, (int)fy, 10, {110, 110, 118, 255});
    fy += 24;

    // Format
    DrawText("Packaging Format", (int)fx, (int)fy, 11, {140, 140, 150, 255});
    fy += 14;
    Rectangle formatRow = SelectRow(fx, fy, fw, 22);
    DrawRectangleRec(formatRow, {26, 26, 28, 255});
    DrawRectangleLinesEx(formatRow, 1, {55, 55, 60, 255});
    std::string formatLabel = format == PackageFormat::LooseFolder
        ? "Loose Content folder (debug)" : "Single file (.starpak embedded)";
    DrawText(formatLabel.c_str(), (int)formatRow.x + 6, (int)formatRow.y + 5, 12, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, formatRow) && !formatPopup.IsOpen()) {
        formatPopup.Open({formatRow.x, formatRow.y + formatRow.height}, fw, {
            {"Single file (.starpak embedded)", true, ""},
            {"Loose Content folder (debug)", true, ""},
        });
    }
    fy += 20;
    DrawText("Single file ships one exe with no loose Content/ folder. Loose folder is faster to re-package for testing.",
              (int)fx, (int)fy, 10, {110, 110, 118, 255});
    fy += 24;

    // Output folder
    DrawText("Output Folder", (int)fx, (int)fy, 11, {140, 140, 150, 255});
    fy += 14;
    float browseWidth = 70.0f;
    Rectangle outputRow = SelectRow(fx, fy, fw - browseWidth - 6, 22);
    DrawRectangleRec(outputRow, {26, 26, 28, 255});
    DrawRectangleLinesEx(outputRow, 1, {55, 55, 60, 255});
    DrawText(outputDir.c_str(), (int)outputRow.x + 6, (int)outputRow.y + 5, 11, {200, 200, 205, 255});
    Rectangle browseButton = {outputRow.x + outputRow.width + 6, fy, browseWidth, 22};
    if (GuiButton(browseButton, "Browse...")) {
        std::string chosen = NativeFileDialog::OpenFolder("Choose Output Folder", outputDir);
        if (!chosen.empty()) outputDir = chosen;
    }
    fy += 34;

    // Package button
    Rectangle packageButton = {fx, fy, fw, 28};
    bool platformReady = selectedPlatform.available;
    if (!platformReady) GuiDisable();
    if (GuiButton(packageButton, "Package")) {
        RunPackage();
    }
    if (!platformReady) GuiEnable();
    fy += 36;

    if (!platformReady) {
        DrawText(("Cannot package for this platform: " + selectedPlatform.unavailableReason).c_str(),
                  (int)fx, (int)fy, 10, {200, 120, 110, 255});
        fy += 18;
    }

    if (hasResult) {
        Color resultColor = lastSuccess ? Color{130, 210, 140, 255} : Color{220, 110, 100, 255};
        DrawText(resultMessage.c_str(), (int)fx, (int)fy, 12, resultColor);
        fy += 18;

        Rectangle logBox = {fx, fy, fw, 140};
        DrawRectangleRec(logBox, {20, 20, 22, 255});
        DrawRectangleLinesEx(logBox, 1, {55, 55, 60, 255});

        std::vector<std::string> lines;
        std::string current;
        for (char c : resultLog) {
            if (c == '\n') { lines.push_back(current); current.clear(); }
            else current += c;
        }
        if (!current.empty()) lines.push_back(current);

        int maxLines = (int)(logBox.height / 12.0f);
        int start = (int)lines.size() > maxLines ? (int)lines.size() - maxLines : 0;
        float ly = logBox.y + 4;
        for (int i = start; i < (int)lines.size(); i++) {
            DrawText(lines[i].c_str(), (int)logBox.x + 6, (int)ly, 10, {170, 170, 178, 255});
            ly += 12;
        }
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
