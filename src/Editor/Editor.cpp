#include "Editor.h"
#include "Core/ProfilerRender.h"
#include "EditorTab.h"
#include "LevelEditorTab.h"
#include "AssetPreviewTab.h"
#include "AnimationPreviewTab.h"
#include "ProjectSettingsTab.h"
#include "EditorSettingsTab.h"
#include "PackageTab.h"
#include "MenuBar.h"
#include "ProjectBrowser.h"
#include "LogPanel.h"
#include "ViewportPanel.h"
#include "Core/ContentPathResolver.h"
#include "Core/Project.h"
#include "Core/EditorAssetHandlerRegistry.h"
#include "AssetPickerPopup.h"
#include "RegistryPickerPopup.h"
#include "ChoicePopup.h"
#include "ConfirmPopup.h"
#include "SearchableListPopup.h"
#include "Core/Widgets/WidgetOverlay.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/DragDropController.h"
#include "ImportDetailsPanel.h"
#include "AboutPanel.h"
#include "LoadingPanel.h"
#include "Core/LayoutSerializer.h"
#include "UITheme.h"
#include "EditorUndo.h"
#include "raylib.h"
#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace fs = std::filesystem;

const float Editor::EDITOR_TAB_BAR_H = 28.0f;
const float Editor::MENU_BAR_H       = 20.0f;

Editor::~Editor() {
    SaveLayout();
    UITheme::Save();
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

    EditorUndo::SetTabRevealer([this](EditorTab* target) {
        for (int i = 0; i < (int)editorTabs.size(); i++) {
            if (editorTabs[i].get() != target) continue;
            if (activeEditorTab != i) {
                activeEditorTab = i;
                target->BuildMenus(menuBar);
            }
            return;
        }
    });

    EditorUndo::SetActiveTabQuery([this]() -> EditorTab* {
        return (activeEditorTab >= 0 && activeEditorTab < (int)editorTabs.size()) ? editorTabs[activeEditorTab].get() : nullptr;
    });

    // Build initial menu from the first (level) tab.
    editorTabs[activeEditorTab]->BuildMenus(menuBar);

    LoadLayout();
    UITheme::Load();
}

LogPanel* Editor::GetLogPanel() const {
    return levelTab ? levelTab->GetLogPanel() : nullptr;
}

void Editor::Render() {
    WidgetInput::BeginFrame();

    if (projectBrowser->IsVisible()) {
        WidgetInput::PopModal(&*menuBar, WidgetInput::kEditorDevice);
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
    const void* menuOwner = &*menuBar;
    WidgetInput::SetInputScope(menuOwner);
    menuBar->RenderBar();
    WidgetInput::SetInputScope(nullptr);
    if (menuBar->activeMenu >= 0) WidgetInput::PushModal(menuOwner, WidgetInput::kEditorDevice);
    else WidgetInput::PopModal(menuOwner, WidgetInput::kEditorDevice);

    float tabBarY  = MENU_BAR_H;
    float contentY = MENU_BAR_H + EDITOR_TAB_BAR_H;
    float contentH = (float)h - contentY;

    // Layer 2: editor tab bar.
    RenderEditorTabBar(0, tabBarY, (float)w);

    // Layer 3: active editor tab panel content.
    if (activeEditorTab >= 0 && activeEditorTab < (int)editorTabs.size()) {
        EditorTab* tab = editorTabs[activeEditorTab].get();
        PROFILE_RENDER_SCOPE_DYN("Tab: " + tab->GetTitle());
        tab->Render(0, contentY, (float)w, contentH);

        if (!tab->IsOpen() && !tab->IsPermanent()) {
            tab->SaveLayout(GetLayoutPath());
            editorTabs.erase(editorTabs.begin() + activeEditorTab);
            if (activeEditorTab >= (int)editorTabs.size())
                activeEditorTab = (int)editorTabs.size() - 1;
            if (activeEditorTab >= 0)
                editorTabs[activeEditorTab]->BuildMenus(menuBar);
        }
    }

    // Layer 4: floating panels, drag ghost, drop highlights (above panel content).
    levelTab->RenderOverlays();
    if (activeEditorTab >= 0 && activeEditorTab < (int)editorTabs.size()) {
        EditorTab* active = editorTabs[activeEditorTab].get();
        if (active != levelTab) active->RenderOverlays();
    }

    // Layer 5: menu popup (above everything).
    WidgetInput::SetInputScope(menuOwner);
    menuBar->RenderPopup();
    WidgetInput::SetInputScope(nullptr);

    // Layer 6: import details popup (above everything but asset picker dropdowns).
    ImportDetailsPanel::Get().Render();

    // Layer 7: about panel (floating modal).
    AboutPanel::Get().Render();

    // Layer 8: asset picker popups (always topmost).
    AssetPickerPopup::FlushDeferredDraws();
    RegistryPickerPopup::FlushDeferredDraws();
    ChoicePopup::FlushDeferredDraws();
    ConfirmPopup::FlushDeferredDraws();
    SearchableListPopup::FlushDeferredDraws();
    WidgetOverlay::TickAndDraw(GetFrameTime(), { 0.0f, 0.0f, (float)w, (float)h });
    DragDrop::Update(GetMousePosition());

    EditorUndo::Tick();

    // Layer 9: loading overlay for async rebuild/packaging (blocks everything else visually).
    LoadingPanel::Get().Render();
}

static bool EditorTabCloseButton(Rectangle r, bool& outHovered) {
    Vector2 mp = GetMousePosition();
    const UIPalette& theme = UITheme::Get();
    outHovered = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(mp, r);
    if (outHovered) DrawRectangleRec(r, theme.danger);
    Color textColor = outHovered ? theme.buttonText : theme.textSecondary;
    int textW = MeasureText("x", 10);
    DrawText("x", (int)(r.x + (r.width - textW) / 2.0f), (int)(r.y + (r.height - 10) / 2.0f), 10, textColor);
    return outHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void Editor::RenderEditorTabBar(float x, float y, float width) {
    const UIPalette& theme = UITheme::Get();

    Rectangle bar = { x, y, width, EDITOR_TAB_BAR_H };
    DrawRectangleRec(bar, theme.tabBar);

    Vector2 mouse = GetMousePosition();
    float tabX = x + 4;
    const float tabPad   = 12.0f;
    const float closeW   = 14.0f;
    const float tabH     = EDITOR_TAB_BAR_H - 2;

    for (int i = 0; i < (int)editorTabs.size(); i++) {
        EditorTab* tab = editorTabs[i].get();
        std::string title = tab->GetTitle();
        if (tab->IsDirty()) title += " *";
        float textW = (float)MeasureText(title.c_str(), 12);
        float tabW  = textW + tabPad * 2 + (tab->IsPermanent() ? 0 : closeW + 4);

        Rectangle tabRect = { tabX, y + 1, tabW, tabH };
        bool hovered = CheckCollisionPointRec(mouse, tabRect);
        bool active  = (i == activeEditorTab);

        // Beveled editor tab style (trapezoid approximated with a rectangle + indicator line).
        DrawRectangleRec(tabRect, active ? theme.surfaceSelected : (hovered ? theme.surfaceHover : theme.tabBar));
        if (active)
            DrawRectangle((int)tabRect.x, (int)(tabRect.y + tabRect.height - 2), (int)tabRect.width, 2, theme.accent);

        DrawText(title.c_str(), (int)(tabX + tabPad), (int)(y + (EDITOR_TAB_BAR_H - 12) / 2.0f + 1), 12, theme.tabText);

        // Close button for non-permanent tabs.
        bool closeClicked = false;
        bool closeHov = false;
        if (!tab->IsPermanent()) {
            Rectangle closeRect = { tabX + tabW - closeW - 3, y + (EDITOR_TAB_BAR_H - closeW) / 2.0f, closeW, closeW };
            closeClicked = EditorTabCloseButton(closeRect, closeHov);

            if (closeClicked) {
                editorTabs[i]->SaveLayout(GetLayoutPath());
                editorTabs.erase(editorTabs.begin() + i);
                if (activeEditorTab >= (int)editorTabs.size())
                    activeEditorTab = (int)editorTabs.size() - 1;
                if (activeEditorTab >= 0)
                    editorTabs[activeEditorTab]->BuildMenus(menuBar);
                return;
            }
        }

        if (hovered && !closeHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && activeEditorTab != i) {
            editorTabs[activeEditorTab]->OnDeactivated();
            activeEditorTab = i;
            editorTabs[activeEditorTab]->OnActivated();
            editorTabs[activeEditorTab]->BuildMenus(menuBar);
        }

        tabX += tabW + 2;
    }

    // Separator line at bottom of tab bar.
    DrawLine((int)x, (int)(y + EDITOR_TAB_BAR_H - 1), (int)(x + width), (int)(y + EDITOR_TAB_BAR_H - 1), theme.splitter);
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
    } else if (extension == ".staranim") {
        for (int i = 0; i < (int)editorTabs.size(); i++) {
            auto* animTab = dynamic_cast<AnimationPreviewTab*>(editorTabs[i].get());
            if (animTab && animTab->GetAssetPath() == fullPath) {
                editorTabs[activeEditorTab]->OnDeactivated();
                activeEditorTab = i;
                editorTabs[activeEditorTab]->OnActivated();
                editorTabs[activeEditorTab]->BuildMenus(menuBar);
                return;
            }
        }
        auto* animTab = new AnimationPreviewTab(fullPath);
        animTab->Initialize();
        editorTabs.emplace_back(animTab);
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
    } else {
        const EditorAssetHandlerInfo* handler = EditorAssetHandlerRegistry::Get().FindHandler(extension);
        if (!handler) return;

        for (int i = 0; i < (int)editorTabs.size(); i++) {
            if (editorTabs[i]->GetAssetPath() == fullPath) {
                editorTabs[activeEditorTab]->OnDeactivated();
                activeEditorTab = i;
                editorTabs[activeEditorTab]->OnActivated();
                editorTabs[activeEditorTab]->BuildMenus(menuBar);
                return;
            }
        }

        EditorServices services;
        services.sharedLogPanel = levelTab->GetLogPanel();
        services.sharedViewportPanel = levelTab->GetViewport();

        EditorTab* tab = handler->createTab(fullPath, services);
        tab->Initialize();
        tab->LoadLayout(GetLayoutPath());
        editorTabs.emplace_back(tab);
        editorTabs[activeEditorTab]->OnDeactivated();
        activeEditorTab = (int)editorTabs.size() - 1;
        editorTabs[activeEditorTab]->OnActivated();
        editorTabs[activeEditorTab]->BuildMenus(menuBar);
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
    for (auto& existingTab : editorTabs) {
        if (auto* levelTab = dynamic_cast<LevelEditorTab*>(existingTab.get())) {
            tab->SetUnsavedLevelCheck([levelTab]() { return levelTab->HasUnsavedLevelChanges(); });
            tab->SetSaveLevelCallback([levelTab]() { levelTab->SaveLevel(); });
            break;
        }
    }
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
    const std::string& mapPath = Project::Get().GetSettings().editorDefaultMapPath;
    if (mapPath.empty()) return;
    std::string resolvedPath = ContentPathResolver::Resolve(mapPath);
    if (!fs::exists(resolvedPath)) return;
    OpenAssetInTab(resolvedPath, ".starmap");
}

void Editor::HandleEditMenuAction(const std::string& item) {
    if (item.rfind("Undo", 0) == 0) { EditorUndo::PerformUndo(); return; }
    if (item.rfind("Redo", 0) == 0) { EditorUndo::PerformRedo(); return; }
    if (item == "Editor Settings")  OpenEditorSettingsTab();
    if (item == "Project Settings") OpenProjectSettingsTab();
}

void Editor::HandleHelpMenuAction(const std::string& item) {
    if (item == "Support") OpenURL("https://ko-fi.com/sirdiabo");
    if (item == "GitHub")  OpenURL("https://github.com/SirDiabo/StarEngine");
    if (item == "About")   AboutPanel::Get().Open();
}

void Editor::HandleWindowMenuAction(const std::string& item) {
    EditorTab* activeTab = (activeEditorTab >= 0 && activeEditorTab < (int)editorTabs.size())
                           ? editorTabs[activeEditorTab].get() : nullptr;
    if (!activeTab) return;

    if (activeTab == levelTab) {
        if (item == "Save Layout As") { levelTab->SaveLayoutAs();         return; }
        if (item == "Load Layout")    { levelTab->LoadLayoutFromDialog();  return; }
    }

    if (item == "Reset Layout") {
        activeTab->ResetLayout();
        activeTab->BuildMenus(menuBar);
        return;
    }

    activeTab->TogglePanelVisible(item);
    activeTab->BuildMenus(menuBar);
}

void Editor::SaveLayout() const {
    for (const auto& tab : editorTabs)
        tab->SaveLayout(GetLayoutPath());
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

void Editor::ResetCancelFlag() {
    if (projectBrowser) projectBrowser->ResetCancelFlag();
}

void Editor::CloseAllAssetEditorTabs() {
    for (int i = (int)editorTabs.size() - 1; i >= 0; i--) {
        if (editorTabs[i]->IsPermanent()) continue;
        editorTabs[i]->SaveLayout(GetLayoutPath());
        editorTabs.erase(editorTabs.begin() + i);
    }
    activeEditorTab = 0;
    if (!editorTabs.empty()) {
        editorTabs[activeEditorTab]->OnActivated();
        editorTabs[activeEditorTab]->BuildMenus(menuBar);
    }
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
        std::string path = editorTabs[i]->GetAssetPath();
        if (!path.empty()) out << "openAsset=" << path << "\n";
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
        } else if (key == "openAsset" && !value.empty()) {
            std::string ext = fs::path(value).extension().string();
            if (!ext.empty()) OpenAssetInTab(value, ext);
        }
    }
}

std::string Editor::GetActiveMapPath() const {
    if (levelTab && levelTab->GetViewport()) {
        return levelTab->GetViewport()->GetMapPath();
    }
    return "";
}
