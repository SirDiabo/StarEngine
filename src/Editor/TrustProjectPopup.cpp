#include "TrustProjectPopup.h"
#include "UITheme.h"
#include "Core/Module.h"
#include "Core/PathSafety.h"
#include "Core/Project.h"
#include "Core/Widgets/WidgetOverlay.h"
#include "Core/Widgets/WidgetElement.h"
#include "PopupWindow.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/ButtonElement.h"
#include <algorithm>
#include <filesystem>
#include <set>
#include <vector>

namespace fs = std::filesystem;

namespace {

const size_t kWrapColumns = 66;
const size_t kMaxListedModules = 10;

std::vector<std::string> WrapText(const std::string& text, size_t columns) {
    std::vector<std::string> lines;
    std::string current;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t space = text.find(' ', pos);
        if (space == std::string::npos) space = text.size();
        std::string word = text.substr(pos, space - pos);
        pos = space + 1;
        if (word.empty()) continue;
        if (!current.empty() && current.size() + 1 + word.size() > columns) {
            lines.push_back(current);
            current.clear();
        }
        if (!current.empty()) current += ' ';
        current += word;
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

std::string ShortenMiddle(const std::string& text, size_t columns) {
    if (text.size() <= columns) return text;
    size_t keep = (columns - 3) / 2;
    return text.substr(0, keep) + "..." + text.substr(text.size() - keep);
}

std::vector<std::string> CollectProjectModuleNames() {
    std::set<std::string> names;
    std::string modulesDir = Project::Get().GetModulesDirectory();
    if (modulesDir.empty()) return {};

    for (const ModuleEntry& entry : ModuleManager::Get().GetEntries()) {
        if (!entry.hasManifest || entry.manifest.sourcePath.empty()) continue;
        if (PathSafety::IsInside(modulesDir, entry.manifest.sourcePath) && fs::path(entry.manifest.sourcePath) != fs::path(modulesDir)) {
            names.insert(entry.manifest.name);
        }
    }

    std::error_code ec;
    for (fs::directory_iterator it(modulesDir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code typeEc;
        if (it->is_directory(typeEc)) names.insert(it->path().filename().string());
    }
    return std::vector<std::string>(names.begin(), names.end());
}

}

void TrustProjectPopup::Open(const std::string& projectName, const std::string& projectRoot) {
    open = true;
    hasPending = false;

    std::vector<std::string> lines;
    for (const std::string& line : WrapText(
             "Building, packaging and importing modules compile this project's modules into StarEngine and run them. "
             "Only continue if you trust where this project came from.", kWrapColumns)) {
        lines.push_back(line);
    }
    lines.push_back("");
    lines.push_back("Project: " + ShortenMiddle(projectName, kWrapColumns - 9));
    lines.push_back("Location: " + ShortenMiddle(projectRoot, kWrapColumns - 10));
    lines.push_back("");

    std::vector<std::string> modules = CollectProjectModuleNames();
    if (modules.empty()) {
        lines.push_back("This project contains no modules of its own.");
    } else {
        lines.push_back("Modules inside this project (their code will be compiled):");
        for (size_t i = 0; i < modules.size() && i < kMaxListedModules; i++) {
            lines.push_back("  - " + ShortenMiddle(modules[i], kWrapColumns - 4));
        }
        if (modules.size() > kMaxListedModules) {
            lines.push_back("  ... and " + std::to_string(modules.size() - kMaxListedModules) + " more");
        }
    }

    const float lineHeight = 16.0f;
    const float topPad = 12.0f;
    const float buttonRow = 52.0f;
    float w = 520.0f;
    float h = topPad + (float)lines.size() * lineHeight + buttonRow;

    const UIPalette& theme = UITheme::Get();

    auto* panel = new WidgetElement();

    float y = topPad;
    for (const std::string& line : lines) {
        auto* text = new TextElement();
        text->text = line;
        text->color = theme.textPrimary;
        text->fontSize = 12.0f;
        text->anchor = {0.0f, 0.0f};
        text->pivot = {0.0f, 0.0f};
        text->offset = {16.0f, y};
        text->size = {w - 32.0f, lineHeight};
        panel->AddChild(text);
        y += lineHeight;
    }

    float btnW = 170.0f;

    auto* cancelBtn = new ButtonElement();
    cancelBtn->defaultColor = theme.buttonBg;
    cancelBtn->hoveredColor = theme.buttonHover;
    cancelBtn->selectedColor = theme.buttonHover;
    cancelBtn->pressedColor = theme.buttonHover;
    cancelBtn->anchor = {0.0f, 0.0f};
    cancelBtn->pivot = {0.0f, 0.0f};
    cancelBtn->offset = {16.0f, h - 38.0f};
    cancelBtn->size = {btnW, 26.0f};
    cancelBtn->onActivated = [this]() {
        pendingTrusted = false;
        hasPending = true;
    };
    auto* cancelLabel = new TextElement();
    cancelLabel->text = "Cancel";
    cancelLabel->color = theme.buttonText;
    cancelLabel->fontSize = 12.0f;
    cancelLabel->anchor = {0.5f, 0.5f};
    cancelLabel->pivot = {0.5f, 0.5f};
    cancelBtn->AddChild(cancelLabel);
    panel->AddChild(cancelBtn);

    auto* trustBtn = new ButtonElement();
    trustBtn->defaultColor = theme.accent;
    trustBtn->hoveredColor = theme.surfaceSelected;
    trustBtn->selectedColor = theme.surfaceSelected;
    trustBtn->pressedColor = theme.surfaceSelected;
    trustBtn->anchor = {0.0f, 0.0f};
    trustBtn->pivot = {0.0f, 0.0f};
    trustBtn->offset = {w - btnW - 16.0f, h - 38.0f};
    trustBtn->size = {btnW, 26.0f};
    trustBtn->onActivated = [this]() {
        pendingTrusted = true;
        hasPending = true;
    };
    auto* trustLabel = new TextElement();
    trustLabel->text = "Trust and Continue";
    trustLabel->color = WHITE;
    trustLabel->fontSize = 12.0f;
    trustLabel->anchor = {0.5f, 0.5f};
    trustLabel->pivot = {0.5f, 0.5f};
    trustBtn->AddChild(trustLabel);
    panel->AddChild(trustBtn);

    PopupWindow::OpenOverlay("Trust this project?", panel, { w, h }, [this]() {
        open = false;
    }, [this]() {
        pendingTrusted = false;
        hasPending = true;
    });
}

void TrustProjectPopup::Close() {
    open = false;
    WidgetOverlay::Close();
}

bool TrustProjectPopup::Render(bool& outTrusted) {
    if (!open) return false;
    if (hasPending) {
        outTrusted = pendingTrusted;
        hasPending = false;
        Close();
        return true;
    }
    return false;
}
