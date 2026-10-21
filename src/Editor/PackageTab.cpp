#include "PackageTab.h"
#include "MenuBar.h"
#include "NativeFileDialog.h"
#include "Core/PackageRunner.h"
#include "LoadingPanel.h"
#include "UITheme.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

static bool PackageTabButton(Rectangle r, const char* label, bool enabled = true) {
    const UIPalette& theme = UITheme::Get();
    bool hovered = enabled && !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, !enabled ? theme.buttonBg : (hovered ? theme.buttonHover : theme.buttonBg));
    DrawRectangleLinesEx(r, 1, theme.buttonBorder);
    int tw = MeasureText(label, 11);
    DrawText(label, (int)(r.x + (r.width - tw) / 2.0f), (int)(r.y + (r.height - 11) / 2.0f), 11,
             enabled ? theme.textPrimary : theme.textSecondary);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static std::string PackageTabFitText(const std::string& text, int fontSize, float maxWidth) {
    if (MeasureText(text.c_str(), fontSize) <= maxWidth) return text;
    std::string out = text;
    while (!out.empty() && MeasureText((out + "...").c_str(), fontSize) > maxWidth) out.pop_back();
    return out + "...";
}

static std::string PackageTabUnavailableReason(const std::string& moduleName) {
    return "Requires the " + moduleName + " module";
}

static void PackageTabWrapLine(const std::string& line, int fontSize, float maxWidth, std::vector<std::string>& out) {
    if (line.empty()) { out.push_back(""); return; }
    std::string current;
    for (char c : line) {
        std::string candidate = current + c;
        if (!current.empty() && MeasureText(candidate.c_str(), fontSize) > maxWidth) {
            out.push_back(current);
            current = std::string(1, c);
        } else {
            current = candidate;
        }
    }
    out.push_back(current);
}

static void PackageTabScrollableLog(Rectangle r, const std::string& content, float& scrollOffset) {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(r, theme.fieldBg);
    DrawRectangleLinesEx(r, 1, theme.fieldBorder);

    float lineHeight = 13.0f;
    float padding = 6.0f;
    int fontSize = 10;
    float maxTextWidth = r.width - padding * 2 - 8.0f;

    std::vector<std::string> lines;
    std::string rawLine;
    for (char c : content) {
        if (c == '\n') {
            PackageTabWrapLine(rawLine, fontSize, maxTextWidth, lines);
            rawLine.clear();
        } else {
            rawLine += c;
        }
    }
    PackageTabWrapLine(rawLine, fontSize, maxTextWidth, lines);

    float contentHeight = (float)lines.size() * lineHeight;
    float visibleHeight = r.height - padding * 2;

    if (!WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), r)) {
        scrollOffset -= GetMouseWheelMove() * lineHeight * 3;
    }

    float maxScroll = (contentHeight > visibleHeight) ? (contentHeight - visibleHeight) : 0.0f;
    if (scrollOffset < 0.0f) scrollOffset = 0.0f;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;

    BeginScissorMode((int)r.x + 1, (int)r.y + 1, (int)r.width - 2, (int)r.height - 2);
    float y = r.y + padding - scrollOffset;
    for (const std::string& l : lines) {
        if (y > r.y - lineHeight && y < r.y + r.height) {
            DrawText(l.c_str(), (int)(r.x + padding), (int)y, fontSize, theme.textSecondary);
        }
        y += lineHeight;
    }
    EndScissorMode();

    if (contentHeight > visibleHeight) {
        float scrollRatio = (maxScroll > 0.0f) ? scrollOffset / maxScroll : 0.0f;
        float barHeight = (visibleHeight / contentHeight) * visibleHeight;
        if (barHeight < 20.0f) barHeight = 20.0f;
        float barY = r.y + padding + scrollRatio * (visibleHeight - barHeight);
        DrawRectangleRec({r.x + r.width - 7, barY, 4, barHeight}, {100, 100, 100, 180});
        DrawRectangleLinesEx({r.x + r.width - 7, barY, 4, barHeight}, 1, {150, 150, 150, 255});
    }
}

void PackageTab::Initialize() {
    title     = "Package Project";
    permanent = false;

    platforms = GetAvailablePackagePlatforms();
    selectedPlatformIndex = -1;

#ifdef _WIN32
    PackagePlatform nativePlatform = PackagePlatform::Windows;
#elif defined(__APPLE__)
    PackagePlatform nativePlatform = PackagePlatform::macOS;
#else
    PackagePlatform nativePlatform = PackagePlatform::Linux;
#endif

    for (int i = 0; i < (int)platforms.size(); i++) {
        if (platforms[i].platform == nativePlatform && platforms[i].available) {
            selectedPlatformIndex = i;
            break;
        }
    }
    if (selectedPlatformIndex < 0) {
        for (int i = 0; i < (int)platforms.size(); i++) {
            if (platforms[i].available) { selectedPlatformIndex = i; break; }
        }
    }

    renderer = Project::Get().GetSettings().renderer;
    outputDir = (fs::path(Project::Get().GetRootPath()) / "Packaged" / PackagePlatformName(platforms[selectedPlatformIndex].platform)).string();
}

static Rectangle SelectRow(float x, float y, float w, float h) {
    return {x, y, w, h};
}

void PackageTab::Render(float x, float y, float width, float height) {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec({x, y, width, height}, theme.panelBg);
    DrawRectangleLinesEx({x, y, width, height}, 1, theme.panelBorder);

    if (packageRunning) {
        LoadingPanel::Get().SetStatusLine(packageTask.GetLatestLine());
        bool taskSuccess = false;
        std::string taskMessage;
        if (packageTask.PollCompletion(taskSuccess, taskMessage)) {
            packageRunning = false;
            LoadingPanel::Get().Hide();
            hasResult = true;
            lastSuccess = taskSuccess;
            resultMessage = taskMessage;
            resultLog = packageTask.GetFullLog();
        }
    }

    Vector2 mp = GetMousePosition();
    float fx = x + 16;
    float fy = y + 16;
    float fw = width - 32 > 640.0f ? 640.0f : width - 32;
    const bool anyPopupOpen = platformPopup.IsOpen() || rendererPopup.IsOpen() ||
                               cipherPopup.IsOpen() || formatPopup.IsOpen() || compressionPopup.IsOpen() || integrityPopup.IsOpen() || unsavedLevelPopup.IsOpen();

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
    CookCipherInfo selectedCipher;
    bool cipherKnown = CookCipherRegistry::Get().Find(cipher, selectedCipher);
    std::string cipherLabel = cipherKnown ? selectedCipher.displayName : cipher + " (not available)";
    DrawText(cipherLabel.c_str(), (int)cipherRow.x + 6, (int)cipherRow.y + 5, 12, theme.textPrimary);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, cipherRow) && !anyPopupOpen) {
        std::vector<ChoiceOption> options;
        cipherChoiceIds.clear();
        for (const CookCipherInfo& c : CookCipherRegistry::Get().GetAll()) {
            bool available = CookCipherRegistry::Get().IsAvailable(c);
            options.push_back({c.displayName, available, available ? "" : PackageTabUnavailableReason(c.moduleName)});
            cipherChoiceIds.push_back(c.id);
        }
        cipherPopup.Open({cipherRow.x, cipherRow.y + cipherRow.height}, fw, options);
    }
    fy += 20;
    DrawText(PackageTabFitText(cipherKnown ? selectedCipher.description : "This method is not registered. Enable the module that provides it.", 10, fw).c_str(),
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

    const bool singleFile = format == PackageFormat::SingleFile;
    DrawText("Compression", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle compressionRow = SelectRow(fx, fy, fw, 22);
    DrawRectangleRec(compressionRow, theme.fieldBg);
    DrawRectangleLinesEx(compressionRow, 1, theme.fieldBorder);
    PackCompressionInfo selectedCompression;
    bool compressionKnown = PackCompressionRegistry::Get().Find(compression, selectedCompression);
    std::string compressionLabel = !singleFile ? "None (single file only)" : (compressionKnown ? selectedCompression.displayName : compression + " (not available)");
    DrawText(compressionLabel.c_str(), (int)compressionRow.x + 6, (int)compressionRow.y + 5, 12,
             singleFile ? theme.textPrimary : theme.textSecondary);
    if (singleFile && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, compressionRow) && !anyPopupOpen) {
        std::vector<ChoiceOption> options;
        compressionChoiceIds.clear();
        for (const PackCompressionInfo& c : PackCompressionRegistry::Get().GetAll()) {
            bool available = PackCompressionRegistry::Get().IsAvailable(c);
            options.push_back({c.displayName, available, available ? "" : PackageTabUnavailableReason(c.moduleName)});
            compressionChoiceIds.push_back(c.id);
        }
        compressionPopup.Open({compressionRow.x, compressionRow.y + compressionRow.height}, fw, options);
    }
    fy += 20;
    DrawText(PackageTabFitText(compressionKnown ? selectedCompression.description : "This method is not registered. Enable the module that provides it.", 10, fw).c_str(),
              (int)fx, (int)fy, 10, theme.textSecondary);
    fy += 24;

    DrawText("Integrity Check", (int)fx, (int)fy, 11, theme.textSecondary);
    fy += 14;
    Rectangle integrityRow = SelectRow(fx, fy, fw, 22);
    DrawRectangleRec(integrityRow, theme.fieldBg);
    DrawRectangleLinesEx(integrityRow, 1, theme.fieldBorder);
    std::string integrityLabel = !singleFile ? "None (single file only)" : (integrity ? "SHA-256 (verified when loaded)" : "None");
    DrawText(integrityLabel.c_str(), (int)integrityRow.x + 6, (int)integrityRow.y + 5, 12,
             singleFile ? theme.textPrimary : theme.textSecondary);
    if (singleFile && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, integrityRow) && !anyPopupOpen) {
        integrityPopup.Open({integrityRow.x, integrityRow.y + integrityRow.height}, fw, {
            {"SHA-256 (verified when loaded)", true, ""},
            {"None", true, ""},
        });
    }
    fy += 20;
    DrawText("Detects damaged or edited package entries. A checksum alone does not stop a determined attacker.",
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
    if (PackageTabButton(browseButton, "Browse...") && !anyPopupOpen) {
        std::string chosen = NativeFileDialog::OpenFolder("Choose Output Folder", outputDir);
        if (!chosen.empty()) outputDir = chosen;
    }
    fy += 34;

    // Package button
    Rectangle packageButton = {fx, fy, fw, 28};
    bool platformReady = selectedPlatform.available;
    if (PackageTabButton(packageButton, "Package", platformReady && !packageRunning) && !anyPopupOpen) {
        if (hasUnsavedLevelChanges && hasUnsavedLevelChanges()) {
            unsavedLevelPopup.Open(
                "The open level has unsaved changes. The packaged build will use the version last saved to disk unless you save first.",
                "Save and Package");
        } else {
            RunPackage();
        }
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
        if (PackageTabButton(copyButton, "Copy Log") && !anyPopupOpen) {
            SetClipboardText(resultLog.c_str());
        }
        fy += 26;

        float logHeight = (y + height) - fy - 12.0f;
        if (logHeight < 100.0f) logHeight = 100.0f;
        float logWidth = width - 32.0f;
        Rectangle logBox = {fx, fy, logWidth, logHeight};
        PackageTabScrollableLog(logBox, resultLog, logScrollOffset);
    }

    int platformChoice = 0;
    if (platformPopup.Render(platformChoice)) {
        selectedPlatformIndex = platformChoice;
        outputDir = (fs::path(Project::Get().GetRootPath()) / "Packaged" / PackagePlatformName(platforms[selectedPlatformIndex].platform)).string();
    }

    int rendererChoice = 0;
    if (rendererPopup.Render(rendererChoice)) {
        renderer = rendererChoice == 1 ? GameRenderer::Software : GameRenderer::Hardware;
        Project::Get().GetSettings().renderer = renderer;
    }

    int cipherChoice = 0;
    if (cipherPopup.Render(cipherChoice)) {
        if (cipherChoice >= 0 && cipherChoice < (int)cipherChoiceIds.size()) cipher = cipherChoiceIds[cipherChoice];
    }

    int formatChoice = 0;
    if (formatPopup.Render(formatChoice)) {
        format = formatChoice == 1 ? PackageFormat::LooseFolder : PackageFormat::SingleFile;
    }

    int compressionChoice = 0;
    if (compressionPopup.Render(compressionChoice)) {
        if (compressionChoice >= 0 && compressionChoice < (int)compressionChoiceIds.size()) compression = compressionChoiceIds[compressionChoice];
    }

    int integrityChoice = 0;
    if (integrityPopup.Render(integrityChoice)) {
        integrity = integrityChoice == 0;
    }

    bool unsavedConfirmed = false;
    if (unsavedLevelPopup.Render(unsavedConfirmed) && unsavedConfirmed) {
        if (saveLevelCallback) saveLevelCallback();
        RunPackage();
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
    options.compression = format == PackageFormat::SingleFile ? compression : PackCompressions::None;
    options.integrity = format == PackageFormat::SingleFile && integrity;

    hasResult = false;
    packageRunning = true;
    LoadingPanel::Get().Show("Packaging Project");

    packageTask.Start([options](const std::function<void(const std::string&)>& reportLine,
                                 std::string& outFinalMessage) {
        PackageResult result = PackageRunner::Package(options, reportLine);
        outFinalMessage = result.success
            ? "Packaged successfully: " + result.outputExecutable
            : "Packaging failed: " + result.error;
        return result.success;
    });
}

void PackageTab::BuildMenus(MenuBar* menuBar) {
    menuBar->SetMenus({
        { "File",   { "Save Project" } },
        { "Edit",   { } },
        { "Window", { } },
        { "Help",   { "Support", "GitHub", "About" } },
    });
}