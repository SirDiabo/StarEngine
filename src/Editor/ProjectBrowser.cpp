#include "ProjectBrowser.h"
#include "Core/EmbeddedAssets.h"
#include "Core/Project.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

void ProjectBrowser::ResetFlags() {
    createConfirmed = false;
    openProject = false;
    visible = false;
    wantsCreate = false;
    WidgetInput::PopModal(this, WidgetInput::kEditorDevice);
}

void ProjectBrowser::Show(Mode m, bool allowCancel) {
    visible = true;
    mode = m;
    wantsCreate = (m == Mode::Create);
    createConfirmed = false;
    openProject = false;
    wantsCancel = false;
    showCancel = allowCancel;
    ScanProjects();
}

void ProjectBrowser::ScanProjects() {
    projects.clear();
    fs::path docsPath = Project::GetProjectsDirectory();

    if (fs::exists(docsPath)) {
        for (const auto& entry : fs::directory_iterator(docsPath)) {
            if (entry.is_directory()) {
                fs::path projFile = entry.path() / (entry.path().filename().string() + ".starproject");
                ProjectInfo info;
                info.name = entry.path().filename().string();
                info.path = projFile.string();
                info.exists = fs::exists(projFile);
                projects.push_back(info);
            }
        }
    }
}

bool ProjectBrowser::IsNameUnique(const std::string& name) const {
    for (const auto& proj : projects) {
        if (proj.name == name) return false;
    }
    return true;
}

void ProjectBrowser::LoadLogoIfNeeded() {
    if (logoLoadAttempted) return;
    logoLoadAttempted = true;

    logo = EmbeddedAssets::Get().LoadTextureAsset("logo.png");
    if (logo.id != 0) logoLoaded = true;
}

void ProjectBrowser::EnsureChrome() {
    if (chrome) return;
    chrome = new Widget();
    auto* root = new WidgetElement();
    chrome->SetRoot(root);

    const UIPalette& theme = UITheme::Get();
    nameField = new TextElement();
    nameField->editable = true;
    nameField->fontSize = 13.0f;
    nameField->backgroundColor = theme.fieldBg;
    nameField->borderColor = theme.fieldBorder;
    nameField->editingBorderColor = theme.accent;
    nameField->onTextChanged = [this](const std::string& text) {
        strncpy(newProjectName, text.c_str(), sizeof(newProjectName) - 1);
        newProjectName[sizeof(newProjectName) - 1] = '\0';
    };
}

void ProjectBrowser::Render() {
    if (!visible) return;
    LoadLogoIfNeeded();
    EnsureChrome();
    WidgetInput::PushModal(this, WidgetInput::kEditorDevice);

    const UIPalette& theme = UITheme::Get();
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    Rectangle modal = { (float)width/2 - 200, (float)height/2 - 150, 400, 300 };

    DrawRectangle(0, 0, width, height, Fade(BLACK, 0.5f));
    DrawRectangleRec(modal, {40, 40, 40, 255});
    DrawRectangleLinesEx(modal, 1, GRAY);

    float titleX = modal.x + 10;
    if (logoLoaded && logo.id != 0) {
        float iconSize = 24.0f;
        DrawTexturePro(logo, {0, 0, (float)logo.width, (float)logo.height},
                       {modal.x + 10, modal.y + 8, iconSize, iconSize}, {0, 0}, 0.0f, WHITE);
        titleX += iconSize + 8;
    }
    DrawText("Project Browser", (int)titleX, (int)modal.y + 10, 20, WHITE);

    WidgetElement* root = chrome->rootElement;
    root->ClearChildren();

    Rectangle newBtn = { (float)(int)modal.x + 10, (float)(int)modal.y + 40, 120, 25 };
    auto* newBtnEl = new ButtonElement();
    newBtnEl->size = { newBtn.width, newBtn.height };
    newBtnEl->offset = { newBtn.x, newBtn.y };
    newBtnEl->defaultColor = theme.buttonBg;
    newBtnEl->hoveredColor = theme.buttonHover;
    newBtnEl->onActivated = [this]() { wantsCreate = true; };
    auto* newBtnLbl = new TextElement();
    newBtnLbl->text = "New Project";
    newBtnLbl->fontSize = 12.0f;
    newBtnLbl->anchor = { 0.5f, 0.5f };
    newBtnLbl->pivot = { 0.5f, 0.5f };
    newBtnEl->AddChild(newBtnLbl);
    root->AddChild(newBtnEl);

    Rectangle txtBox = { (float)(int)modal.x + 140, (float)(int)modal.y + 40, 250, 25 };
    if (wantsCreate) {
        nameField->size = { txtBox.width, txtBox.height };
        nameField->offset = { txtBox.x, txtBox.y };
        root->AddChild(nameField);
        if (!nameField->isEditing) nameField->StartEditing();
    }

    float y = (float)(int)modal.y + 80;
    for (size_t i = 0; i < projects.size() && y < (int)modal.y + 260; i++) {
        Rectangle btn = { (float)(int)modal.x + 10, y, 380, 25 };
        std::string label = projects[i].name + (projects[i].exists ? "" : " (missing .starproject)");

        auto* rowBtn = new ButtonElement();
        rowBtn->size = { btn.width, btn.height };
        rowBtn->offset = { btn.x, btn.y };
        rowBtn->defaultColor = theme.buttonBg;
        rowBtn->hoveredColor = theme.buttonHover;
        bool exists = projects[i].exists;
        std::string path = projects[i].path;
        rowBtn->onActivated = [this, exists, path]() {
            if (exists) {
                selectedPath = path;
                openProject = true;
            }
        };
        auto* rowLbl = new TextElement();
        rowLbl->text = label;
        rowLbl->fontSize = 12.0f;
        rowLbl->anchor = { 0.0f, 0.5f };
        rowLbl->pivot = { 0.0f, 0.5f };
        rowLbl->offset = { 8.0f, 0.0f };
        rowBtn->AddChild(rowLbl);
        root->AddChild(rowBtn);

        y += 30;
    }

    if (showCancel) {
        Rectangle cancelBtn = { (float)(int)modal.x + 10, (float)(int)modal.y + 270, 100, 25 };
        auto* cancelBtnEl = new ButtonElement();
        cancelBtnEl->size = { cancelBtn.width, cancelBtn.height };
        cancelBtnEl->offset = { cancelBtn.x, cancelBtn.y };
        cancelBtnEl->defaultColor = theme.buttonBg;
        cancelBtnEl->hoveredColor = theme.buttonHover;
        cancelBtnEl->onActivated = [this]() {
            visible = false;
            wantsCancel = true;
            WidgetInput::PopModal(this, WidgetInput::kEditorDevice);
        };
        auto* cancelLbl = new TextElement();
        cancelLbl->text = "Cancel";
        cancelLbl->fontSize = 12.0f;
        cancelLbl->anchor = { 0.5f, 0.5f };
        cancelLbl->pivot = { 0.5f, 0.5f };
        cancelBtnEl->AddChild(cancelLbl);
        root->AddChild(cancelBtnEl);
    }

    bool hasName = strlen(newProjectName) > 0;
    bool nameIsUnique = hasName && IsNameUnique(std::string(newProjectName));
    if (nameIsUnique) {
        Rectangle createBtn = { (float)(int)modal.x + 290, (float)(int)modal.y + 270, 100, 25 };
        auto* createBtnEl = new ButtonElement();
        createBtnEl->size = { createBtn.width, createBtn.height };
        createBtnEl->offset = { createBtn.x, createBtn.y };
        createBtnEl->defaultColor = theme.buttonBg;
        createBtnEl->hoveredColor = theme.buttonHover;
        createBtnEl->onActivated = [this]() {
            createConfirmed = true;
            wantsCreate = false;
        };
        auto* createLbl = new TextElement();
        createLbl->text = "Create";
        createLbl->fontSize = 12.0f;
        createLbl->anchor = { 0.5f, 0.5f };
        createLbl->pivot = { 0.5f, 0.5f };
        createBtnEl->AddChild(createLbl);
        root->AddChild(createBtnEl);
    }

    WidgetInput::SetInputScope(this);
    float dt = GetFrameTime();
    chrome->Tick(dt, modal, 1.0f);
    WidgetInput::ProcessInput(chrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    chrome->Draw(modal, 1.0f);
    WidgetInput::SetInputScope(nullptr);
}
