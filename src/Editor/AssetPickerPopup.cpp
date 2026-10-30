#include "AssetPickerPopup.h"
#include "Runtime/Audio/AudioSource.h"
#include <algorithm>
#include <sstream>
#include "Core/Project.h"
#include "Core/Widgets/WidgetOverlay.h"
#include "Core/Widgets/SearchListElement.h"
#include "UITheme.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void AssetPickerPopup::Open(Vector2 anchorPos, const std::string& ext) {
    OpenWithExtras(anchorPos, ext, {});
}

void AssetPickerPopup::OpenWithExtras(Vector2 anchorPos, const std::string& ext,
                                       const std::vector<std::pair<std::string, std::string>>& extras) {
    open = true;
    hasPending = false;
    extension = ext;
    displayNames.clear();
    contentPaths.clear();

    for (const auto& extra : extras) {
        displayNames.push_back(extra.first);
        contentPaths.push_back(extra.second);
    }

    std::vector<std::string> accepted;
    {
        std::stringstream filterStream(extension);
        std::string part;
        while (std::getline(filterStream, part, ';')) {
            if (part == "@audio") {
                for (const std::string& audioExt : AudioSourceRegistry::Get().GetExtensions()) accepted.push_back(audioExt);
            } else if (!part.empty()) {
                accepted.push_back(part);
            }
        }
    }

    fs::path contentRoot = fs::path(Project::Get().GetRootPath()) / "Content";
    if (fs::exists(contentRoot)) {
        for (const auto& entry : fs::recursive_directory_iterator(contentRoot)) {
            if (entry.is_directory()) continue;
            if (std::find(accepted.begin(), accepted.end(), entry.path().extension().string()) == accepted.end()) continue;

            fs::path relative = fs::relative(entry.path(), contentRoot);
            displayNames.push_back(relative.stem().string());
            contentPaths.push_back(relative.generic_string());
        }
    }

    float rowH = 20.0f;
    float contentHeight = (float)(displayNames.size() + 1) * rowH;
    float maxContentHeight = 260.0f;
    float height = 26.0f + std::min(contentHeight, maxContentHeight) + 4.0f;
    area = { anchorPos.x, anchorPos.y, 220.0f, height };

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    if (area.x + area.width > screenW) area.x = screenW - area.width;
    if (area.x < 0) area.x = 0;
    if (area.y + area.height > screenH) area.y = screenH - area.height;
    if (area.y < 0) area.y = 0;

    auto* list = new SearchListElement();
    list->rowHeight = rowH;
    const UIPalette& theme = UITheme::Get();
    list->backgroundColor = theme.panelBg;
    list->rowColor = theme.textPrimary;
    list->rowHoveredColor = theme.surfaceHover;
    list->SetFieldColors(theme.fieldBg, theme.fieldBorder, theme.accent, theme.textPrimary, theme.textSecondary);

    std::vector<SearchListElement::Entry> listEntries;
    listEntries.reserve(displayNames.size() + 1);

    SearchListElement::Entry noneEntry;
    noneEntry.id = "";
    noneEntry.label = "(None)";
    listEntries.push_back(noneEntry);

    for (size_t i = 0; i < displayNames.size(); i++) {
        SearchListElement::Entry entry;
        entry.id = contentPaths[i];
        entry.label = displayNames[i];
        listEntries.push_back(entry);
    }
    list->SetEntries(listEntries);

    list->onEntrySelected = [this](const std::string& id) {
        pendingPath = id;
        hasPending = true;
    };

    WidgetOverlay::Open(list, { area.x, area.y }, { area.width, area.height }, [this]() {
        open = false;
    });
}

void AssetPickerPopup::Close() {
    open = false;
    WidgetOverlay::Close();
}

bool AssetPickerPopup::Render(std::string& outPath) {
    if (!open) return false;
    if (hasPending) {
        outPath = pendingPath;
        hasPending = false;
        Close();
        return true;
    }
    return false;
}

void AssetPickerPopup::FlushDeferredDraws() {
}
