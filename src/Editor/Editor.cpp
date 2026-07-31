#include "Editor.h"
#include "EditorTab.h"
#include "LevelEditorTab.h"
#include "StargraphEditorTab.h"
#include "AssetPreviewTab.h"
#include "ProjectSettingsTab.h"
#include "EditorSettingsTab.h"
#include "PackageTab.h"
#include "MenuBar.h"
#include "ProjectBrowser.h"
#include "LogPanel.h"
#include "ViewportPanel.h"
#include "Core/ContentPathResolver.h"
#include "Core/Project.h"
#include "AssetPickerPopup.h"
#include "RegistryPickerPopup.h"
#include "ChoicePopup.h"
#include "ConfirmPopup.h"
#include "ImportDetailsPanel.h"
#include "AboutPanel.h"
#include "Core/LayoutSerializer.h"
#include "raygui.h"
#include "raylib.h"
#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace fs = std::filesystem;

const float Editor::EDITOR_TAB_BAR_H = 28.0f;
const float Editor::MENU_BAR_H       = 20.0f;

static const Color COLOR_EDITOR_TAB_BAR  = { 30, 30, 33, 255 };
static const Color COLOR_EDITOR_TAB_ACT  = { 50, 50, 58, 255 };
static const Color COLOR_EDITOR_TAB_HOV  = { 40, 40, 46, 255 };
static const Color COLOR_EDITOR_TAB_IDLE = { 30, 30, 33, 255 };
static const Color COLOR_EDITOR_TAB_TEXT = { 200, 200, 210, 255 };
static const Color COLOR_EDITOR_TAB_LINE = { 70, 120, 220, 255 };

Editor::~Editor() {
    SaveLayout();
    delete menuBar;
    delete projectBrowser;
}

void Editor::Initialize() {
    auto* tab = new LevelEditorTab();
    tab->onAssetOpened = [this](const std::string& path, const std::string& ext) {
        OpenAssetInTab(path, ext);
    };
    tab->Initialize();
    tab->SetPackageCallback([this]() { OpenPackageTab(); });
    levelTab = tab;
    editorTabs.emplace_back(tab);

    menuBar = new MenuBar();
    menuBar->Initialize();

    projectBrowser = new ProjectBrowser();

    menuBar->SetActionCallback([this](const std::string& menu, const std::string& item) {
        if (menu == "Help")   HandleHelpMenuAction(item);
        if (menu == "Window")  HandleWindowMenuAction(item);
        if (menu == "Edit")    HandleEditMenuAction(item);
        if (menu == "Package") HandlePackageMenuAction(item);
    });

    // Build initial menu from the first (level) tab.
    editorTabs[activeEditorTab]->BuildMenus(menuBar);

    LoadLayout();
}

LogPanel* Editor::GetLogPanel() const {
    return levelTab ? levelTab->GetLogPanel() : nullptr;
}

void Editor::Render() {
    if (projectBrowser->IsVisible()) {
        projectBrowser->Render();
        return;
    }

    int w = GetScreenWidth();
    int h = GetScreenHeight();

    if (menuBar->activeMenu < 0 && !projectBrowser->IsVisible()) {
        EditorTab* tab = (activeEditorTab >= 0 && activeEditorTab < (int)editorTabs.size())
                         ? editorTabs[activeEditorTab].get() : nullptr;
        if (tab) tab->BuildMenus(menuBar);
    }

    // Layer 1: menu bar background and buttons.
    menuBar->RenderBar();

    float tabBarY  = MENU_BAR_H;
    float contentY = MENU_BAR_H + EDITOR_TAB_BAR_H;
    float contentH = (float)h - contentY;

    // Layer 2: editor tab bar.
    RenderEditorTabBar(0, tabBarY, (float)w);

    // Layer 3: active editor tab panel content.
    if (activeEditorTab >= 0 && activeEditorTab < (int)editorTabs.size()) {
        EditorTab* tab = editorTabs[activeEditorTab].get();
        tab->Render(0, contentY, (float)w, contentH);

        if (!tab->IsOpen() && !tab->IsPermanent()) {
            editorTabs.erase(editorTabs.begin() + activeEditorTab);
            if (activeEditorTab >= (int)editorTabs.size())
                activeEditorTab = (int)editorTabs.size() - 1;
            if (activeEditorTab >= 0)
                editorTabs[activeEditorTab]->BuildMenus(menuBar);
        }
    }

    // Layer 4: floating panels, drag ghost, drop highlights (above panel content).
    levelTab->RenderOverlays();

    // Layer 5: menu popup (above everything).
    menuBar->RenderPopup();

    // Layer 6: import details popup (above everything but asset picker dropdowns).
    ImportDetailsPanel::Get().Render();

    // Layer 7: about panel (floating modal).
    AboutPanel::Get().Render();

    // Layer 8: asset picker popups (always topmost).
    AssetPickerPopup::FlushDeferredDraws();
    RegistryPickerPopup::FlushDeferredDraws();
    ChoicePopup::FlushDeferredDraws();
    ConfirmPopup::FlushDeferredDraws();
}

void Editor::RenderEditorTabBar(float x, float y, float width) {
    Rectangle bar = { x, y, width, EDITOR_TAB_BAR_H };
    DrawRectangleRec(bar, COLOR_EDITOR_TAB_BAR);

    Vector2 mouse = GetMousePosition();
    float tabX = x + 4;
    const float tabPad   = 12.0f;
    const float closeW   = 14.0f;
    const float tabH     = EDITOR_TAB_BAR_H - 2;

    for (int i = 0; i < (int)editorTabs.size(); i++) {
        EditorTab* tab = editorTabs[i].get();
        const std::string& title = tab->GetTitle();
        float textW = (float)MeasureText(title.c_str(), 12);
        float tabW  = textW + tabPad * 2 + (tab->IsPermanent() ? 0 : closeW + 4);

        Rectangle tabRect = { tabX, y + 1, tabW, tabH };
        bool hovered = CheckCollisionPointRec(mouse, tabRect);
        bool active  = (i == activeEditorTab);

        // Beveled editor tab style (trapezoid approximated with a rectangle + indicator line).
        DrawRectangleRec(tabRect, active ? COLOR_EDITOR_TAB_ACT : (hovered ? COLOR_EDITOR_TAB_HOV : COLOR_EDITOR_TAB_IDLE));
        if (active)
            DrawRectangle((int)tabRect.x, (int)(tabRect.y + tabRect.height - 2), (int)tabRect.width, 2, COLOR_EDITOR_TAB_LINE);

        DrawText(title.c_str(), (int)(tabX + tabPad), (int)(y + (EDITOR_TAB_BAR_H - 12) / 2.0f + 1), 12, COLOR_EDITOR_TAB_TEXT);

        // Close button for non-permanent tabs.
        if (!tab->IsPermanent()) {
            Rectangle closeRect = { tabX + tabW - closeW - 3, y + (EDITOR_TAB_BAR_H - closeW) / 2.0f, closeW, closeW };
            bool closeHov = CheckCollisionPointRec(mouse, closeRect);
            if (closeHov) DrawRectangleRec(closeRect, { 180, 60, 60, 200 });
            DrawText("x", (int)closeRect.x + 3, (int)closeRect.y + 2, 10, closeHov ? WHITE : Color{ 120, 120, 120, 255 });

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && closeHov) {
                editorTabs.erase(editorTabs.begin() + i);
                if (activeEditorTab >= (int)editorTabs.size())
                    activeEditorTab = (int)editorTabs.size() - 1;
                if (activeEditorTab >= 0)
                    editorTabs[activeEditorTab]->BuildMenus(menuBar);
                return;
            }
        }

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && activeEditorTab != i) {
            editorTabs[activeEditorTab]->OnDeactivated();
            activeEditorTab = i;
            editorTabs[activeEditorTab]->OnActivated();
            editorTabs[activeEditorTab]->BuildMenus(menuBar);
        }

        tabX += tabW + 2;
    }

    // Separator line at bottom of tab bar.
    DrawLine((int)x, (int)(y + EDITOR_TAB_BAR_H - 1), (int)(x + width), (int)(y + EDITOR_TAB_BAR_H - 1), { 22, 22, 25, 255 });
}

void Editor::OpenAssetInTab(const std::string& fullPath, const std::string& extension) {
    if (extension == ".starasset") {
        for (int i = 0; i < (int)editorTabs.size(); i++) {
            auto* previewTab = dynamic_cast<AssetPreviewTab*>(editorTabs[i].get());
            if (previewTab && previewTab->GetAssetPath() == fullPath) {
                editorTabs[activeEditorTab]->OnDeactivated();
                activeEditorTab = i;
                editorTabs[activeEditorTab]->OnActivated();
                editorTabs[activeEditorTab]->BuildMenus(menuBar);
                return;
            }
        }
        auto* previewTab = new AssetPreviewTab(fullPath);
        previewTab->Initialize();
        editorTabs.emplace_back(previewTab);
        editorTabs[activeEditorTab]->OnDeactivated();
        activeEditorTab = (int)editorTabs.size() - 1;
        editorTabs[activeEditorTab]->OnActivated();
        editorTabs[activeEditorTab]->BuildMenus(menuBar);
    } else if (extension == ".staractor" || extension == ".stargraph") {
        // Check if already open.
        for (int i = 0; i < (int)editorTabs.size(); i++) {
            auto* sgTab = dynamic_cast<StargraphEditorTab*>(editorTabs[i].get());
            if (sgTab && sgTab->GetAssetPath() == fullPath) {
                editorTabs[activeEditorTab]->OnDeactivated();
                activeEditorTab = i;
                editorTabs[activeEditorTab]->OnActivated();
                editorTabs[activeEditorTab]->BuildMenus(menuBar);
                return;
            }
        }
        auto* sgTab = new StargraphEditorTab(fullPath);
        sgTab->SetSharedLogPanel(levelTab->GetLogPanel());
        sgTab->SetViewportPanel(levelTab->GetViewport());
        sgTab->Initialize();
        editorTabs.emplace_back(sgTab);
        editorTabs[activeEditorTab]->OnDeactivated();
        activeEditorTab = (int)editorTabs.size() - 1;
        editorTabs[activeEditorTab]->OnActivated();
        editorTabs[activeEditorTab]->BuildMenus(menuBar);
    } else if (extension == ".starmap") {
        // Map loads into the level editor tab.
        levelTab->GetViewport()->LoadMap(fullPath);
        // Switch to level editor.
        for (int i = 0; i < (int)editorTabs.size(); i++) {
            if (editorTabs[i].get() == levelTab) {
                editorTabs[activeEditorTab]->OnDeactivated();
                activeEditorTab = i;
                editorTabs[activeEditorTab]->OnActivated();
                editorTabs[activeEditorTab]->BuildMenus(menuBar);
                break;
            }
        }
    }
}

void Editor::OpenEditorSettingsTab() {
    for (int i = 0; i < (int)editorTabs.size(); i++) {
        if (dynamic_cast<EditorSettingsTab*>(editorTabs[i].get())) {
            editorTabs[activeEditorTab]->OnDeactivated();
            activeEditorTab = i;
            editorTabs[activeEditorTab]->OnActivated();
            editorTabs[activeEditorTab]->BuildMenus(menuBar);
            return;
        }
    }
    auto* tab = new EditorSettingsTab();
    tab->Initialize();
    editorTabs.emplace_back(tab);
    editorTabs[activeEditorTab]->OnDeactivated();
    activeEditorTab = (int)editorTabs.size() - 1;
    editorTabs[activeEditorTab]->OnActivated();
    editorTabs[activeEditorTab]->BuildMenus(menuBar);
}

void Editor::OpenProjectSettingsTab() {
    for (int i = 0; i < (int)editorTabs.size(); i++) {
        if (dynamic_cast<ProjectSettingsTab*>(editorTabs[i].get())) {
            editorTabs[activeEditorTab]->OnDeactivated();
            activeEditorTab = i;
            editorTabs[activeEditorTab]->OnActivated();
            editorTabs[activeEditorTab]->BuildMenus(menuBar);
            return;
        }
    }
    auto* tab = new ProjectSettingsTab();
    tab->Initialize();
    editorTabs.emplace_back(tab);
    editorTabs[activeEditorTab]->OnDeactivated();
    activeEditorTab = (int)editorTabs.size() - 1;
    editorTabs[activeEditorTab]->OnActivated();
    editorTabs[activeEditorTab]->BuildMenus(menuBar);
}

void Editor::OpenPackageTab() {
    for (int i = 0; i < (int)editorTabs.size(); i++) {
        if (dynamic_cast<PackageTab*>(editorTabs[i].get())) {
            editorTabs[activeEditorTab]->OnDeactivated();
            activeEditorTab = i;
            editorTabs[activeEditorTab]->OnActivated();
            editorTabs[activeEditorTab]->BuildMenus(menuBar);
            return;
        }
    }
    auto* tab = new PackageTab();
    tab->Initialize();
    editorTabs.emplace_back(tab);
    editorTabs[activeEditorTab]->OnDeactivated();
    activeEditorTab = (int)editorTabs.size() - 1;
    editorTabs[activeEditorTab]->OnActivated();
    editorTabs[activeEditorTab]->BuildMenus(menuBar);
}

void Editor::HandlePackageMenuAction(const std::string& item) {
    if (item == "Package Project") OpenPackageTab();
}

void Editor::OpenDefaultMapIfSet() {
    const std::string& mapPath = Project::Get().GetSettings().defaultMapPath;
    if (mapPath.empty()) return;
    std::string contentRoot = Project::Get().GetRootPath() + "/Content";
    ContentPathResolver::SetContentRoot(contentRoot);
    std::string resolvedPath = ContentPathResolver::Resolve(mapPath);
    if (!fs::exists(resolvedPath)) return;
    OpenAssetInTab(resolvedPath, ".starmap");
}

void Editor::HandleEditMenuAction(const std::string& item) {
    if (item == "Editor Settings")  OpenEditorSettingsTab();
    if (item == "Project Settings") OpenProjectSettingsTab();
}

void Editor::HandleHelpMenuAction(const std::string& item) {
    if (item == "Support") OpenURL("https://ko-fi.com/sirdiabo");
    if (item == "GitHub")  OpenURL("https://github.com/SirDiabo/StarEngine");
    if (item == "About")   AboutPanel::Get().Open();
}

void Editor::HandleWindowMenuAction(const std::string& item) {
    if (item == "Reset Layout")      { levelTab->ResetLayout();          levelTab->BuildMenus(menuBar); return; }
    if (item == "Save Layout As")    { levelTab->SaveLayoutAs();         return; }
    if (item == "Load Layout")       { levelTab->LoadLayoutFromDialog();  return; }
    // Toggle named panels in the level editor's dock area.
    static const std::vector<std::string> panelNames = {
        "Viewport", "Details", "Outliner", "Content Browser", "Logs", "Modules"
    };
    for (const auto& name : panelNames) {
        if (item == name) {
            levelTab->TogglePanelVisible(name);
            levelTab->BuildMenus(menuBar);
            return;
        }
    }
}

void Editor::SaveLayout() const {
    levelTab->SaveLayout(GetLayoutPath());
}

void Editor::LoadLayout() {
    levelTab->LoadLayout(GetLayoutPath());
}

std::string Editor::GetLayoutPath() const {
    std::string home;
#ifdef _WIN32
    char* buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, "USERPROFILE") == 0 && buf) {
        home = buf;
        free(buf);
    } else {
        home = ".";
    }
#else
    const char* h = getenv("HOME");
    home = h ? h : ".";
#endif
    fs::path dir = fs::path(home) / ".starengine";
    fs::create_directories(dir);
    return (dir / "layout.cfg").string();
}

// Project browser delegation

void Editor::ShowProjectBrowser(bool forNew, bool allowCancel) {
    projectBrowser->Show(forNew ? ProjectBrowser::Mode::Create : ProjectBrowser::Mode::Open, allowCancel);
}

bool Editor::WantsNewProject() {
    if (projectBrowser && projectBrowser->WantsNewProject()) return true;
    if (menuBar && menuBar->wantsNewProject) {
        ShowProjectBrowser(true, true);
        menuBar->wantsNewProject = false;
    }
    return false;
}

bool Editor::WantsOpenProject() {
    if (projectBrowser && projectBrowser->WantsOpenProject()) return true;
    if (menuBar && menuBar->wantsOpenProject) {
        ShowProjectBrowser(false, true);
        menuBar->wantsOpenProject = false;
    }
    return false;
}

bool Editor::WantsSaveProject() {
    return menuBar && menuBar->wantsSaveProject;
}

bool Editor::WantsCancel() {
    return projectBrowser && projectBrowser->WantsCancel();
}

std::string Editor::GetNewProjectName() const {
    return (projectBrowser && projectBrowser->WantsNewProject()) ? projectBrowser->GetNewProjectName() : "";
}

std::string Editor::GetOpenProjectPath() const {
    return projectBrowser ? projectBrowser->GetSelectedPath() : "";
}

void Editor::ResetNewProjectFlag() {
    if (menuBar) menuBar->wantsNewProject = false;
    projectBrowser->ResetFlags();
}

void Editor::ResetSaveProjectFlag() {
    if (menuBar) menuBar->wantsSaveProject = false;
}

void Editor::SaveState() const {
    fs::path dir = fs::path(GetLayoutPath()).parent_path();
    fs::create_directories(dir);
    fs::path statePath = dir / "state.cfg";

    std::ofstream out(statePath);
    if (!out.is_open()) return;

    out << "activeMap=" << GetActiveMapPath() << "\n";

    for (size_t i = 0; i < editorTabs.size(); i++) {
        if (editorTabs[i]->IsPermanent()) continue;
        auto* sgTab = dynamic_cast<StargraphEditorTab*>(editorTabs[i].get());
        if (sgTab) {
            out << "openStargraph=" << sgTab->GetAssetPath() << "\n";
        }
    }
}

void Editor::RestoreState() {
    fs::path dir = fs::path(GetLayoutPath()).parent_path();
    fs::path statePath = dir / "state.cfg";

    std::ifstream in(statePath);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (key == "activeMap" && !value.empty()) {
            OpenAssetInTab(value, ".starmap");
        } else if (key == "openStargraph" && !value.empty()) {
            OpenAssetInTab(value, ".stargraph");
        }
    }
}

std::string Editor::GetActiveMapPath() const {
    if (levelTab && levelTab->GetViewport()) {
        return levelTab->GetViewport()->GetMapPath();
    }
    return "";
}