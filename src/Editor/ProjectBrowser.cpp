#include "ProjectBrowser.h"
#include "Core/EmbeddedAssets.h"
#include "Core/Project.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/VerticalBoxElement.h"
#include "Core/Widgets/HorizontalBoxElement.h"
#include "Core/Widgets/SpacerElement.h"
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
    auto* root = new VerticalBoxElement();
    root->childSpacing = 5.0f;
    chrome->SetRoot(root);

    topRow = new HorizontalBoxElement();
    topRow->size = { 0.0f, 25.0f };
    root->AddChild(topRow);

    listScroll = new VerticalBoxElement();
    listScroll->scrollEnabled = true;
    listScroll->size = { 0.0f, 190.0f };
    root->AddChild(listScroll);

    bottomRow = new HorizontalBoxElement();
    bottomRow->size = { 0.0f, 25.0f };
    root->AddChild(bottomRow);
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

    Rectangle inner = { modal.x + 10, modal.y + 40, modal.width - 20, modal.height - 50 };

    VerticalBoxElement* root = static_cast<VerticalBoxElement*>(chrome->rootElement);
    topRow->ClearChildren();
    listScroll->ClearChildren();
    bottomRow->ClearChildren();

    auto* newBtnEl = new ButtonElement();
    newBtnEl->size = { 120.0f, 25.0f };
    newBtnEl->defaultColor = theme.buttonBg;
    newBtnEl->hoveredColor = theme.buttonHover;
    newBtnEl->onActivated = [this]() { wantsCreate = true; };
    auto* newBtnLbl = new TextElement();
    newBtnLbl->text = "New Project";
    newBtnLbl->fontSize = 12.0f;
    newBtnLbl->anchor = { 0.5f, 0.5f };
    newBtnLbl->pivot = { 0.5f, 0.5f };
    newBtnEl->AddChild(newBtnLbl);
    topRow->AddChild(newBtnEl);

    if (wantsCreate) {
        auto* spacer1 = new SpacerElement();
        spacer1->size = { 10.0f, 0.0f };
        topRow->AddChild(spacer1);

        nameField = new TextElement();
        nameField->editable = true;
        nameField->fontSize = 13.0f;
        nameField->backgroundColor = theme.fieldBg;
        nameField->borderColor = theme.fieldBorder;
        nameField->editingBorderColor = theme.accent;
        nameField->size = { 250.0f, 25.0f };
        nameField->text = newProjectName;
        nameField->onTextChanged = [this](const std::string& text) {
            strncpy(newProjectName, text.c_str(), sizeof(newProjectName) - 1);
            newProjectName[sizeof(newProjectName) - 1] = '\0';
        };
        topRow->AddChild(nameField);
        if (!nameField->isEditing) nameField->StartEditing();
    }

    for (size_t i = 0; i < projects.size(); i++) {
        std::string label = projects[i].name + (projects[i].exists ? "" : " (missing .starproject)");

        auto* rowBtn = new ButtonElement();
        rowBtn->size = { 0.0f, 25.0f };
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

        auto* rowVBox = new VerticalBoxElement();
        rowVBox->AddChild(rowBtn);

        auto* spacer = new SpacerElement();
        spacer->size = { 0.0f, 2.0f };
        rowVBox->AddChild(spacer);

        listScroll->AddChild(rowVBox);
    }

    if (showCancel) {
        auto* cancelBtnEl = new ButtonElement();
        cancelBtnEl->size = { 100.0f, 25.0f };
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
        bottomRow->AddChild(cancelBtnEl);
    }

    bool hasName = strlen(newProjectName) > 0;
    bool nameIsUnique = hasName && IsNameUnique(std::string(newProjectName));
    if (nameIsUnique) {
        auto* createBtnEl = new ButtonElement();
        createBtnEl->size = { 100.0f, 25.0f };
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
        bottomRow->AddChild(createBtnEl);
    }

    WidgetInput::SetInputScope(this);
    float dt = GetFrameTime();
    chrome->Tick(dt, inner, 1.0f);
    WidgetInput::ProcessInput(chrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    chrome->Draw(inner, 1.0f);
    WidgetInput::SetInputScope(nullptr);
}
