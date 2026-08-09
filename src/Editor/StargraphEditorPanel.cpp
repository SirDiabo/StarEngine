#include "StargraphEditorPanel.h"
#include "ViewportPanel.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "raygui.h"

const float StargraphEditorPanel::minNodeWidth   = 160.0f;
const float StargraphEditorPanel::pinHeight      = 22.0f;
const float StargraphEditorPanel::pinRadius      = 5.0f;
const float StargraphEditorPanel::headerHeight   = 26.0f;
const float StargraphEditorPanel::leftPanelWidth = 200.0f;
const float StargraphEditorPanel::rightPanelWidth= 210.0f;
const float StargraphEditorPanel::toolbarHeight  = 30.0f;
const float StargraphEditorPanel::componentOutlinerHeight = 130.0f;
const float StargraphEditorPanel::primitiveNodeHeight = 26.0f;

void StargraphEditorPanel::Initialize() {
    name = "Stargraph";
}

bool StargraphEditorPanel::Open(const std::string& path) {
    if (!graph.LoadFromFile(path)) return false;
    filePath = path;
    isOpen = true;
    isDirty = false;
    selectedNodeId = -1;
    selectedNodeIds.clear();
    boxSelecting = false;
    linkFromPin = -1;
    nodeMenuOpen = false;
    editingPinId = 0;
    listeningForKeyPinId = 0;
    keyPickerOpen = false;
    keyPickerPinId = 0;
    keyPickerSearch[0] = '\0';
    audioPathPicker.Close();
    canvasState.scroll = {0, 0};
    canvasState.didPan = false;
    variableDragActive = false;
    variableDragIndex = -1;
    componentDragActive = false;
    componentDragIndex = -1;
    return true;
}

void StargraphEditorPanel::Close() {
    isOpen = false;
    filePath.clear();
    isDirty = false;
    variableDragActive = false;
    variableDragIndex = -1;
    componentDragActive = false;
    componentDragIndex = -1;
    keyPickerOpen = false;
    keyPickerPinId = 0;
    listeningForKeyPinId = 0;
    audioPathPicker.Close();
}

void StargraphEditorPanel::SaveGraph() {
    if (!graph.SaveToFile(filePath)) return;
    isDirty = false;
    if (viewportPanel) viewportPanel->InvalidateClassGraph(filePath);
}

void StargraphEditorPanel::RenderToolbar() {
    const UIPalette& theme = UITheme::Get();

    Rectangle toolbar = {bounds.x, bounds.y, bounds.width, toolbarHeight};
    DrawRectangleRec(toolbar, theme.tabBar);
    DrawLine((int)toolbar.x, (int)(toolbar.y + toolbar.height),
             (int)(toolbar.x + toolbar.width), (int)(toolbar.y + toolbar.height), theme.panelBorder);

    float btnY = toolbar.y + 4;
    float btnLeft = bounds.x + 8;

    Rectangle saveBtn = {btnLeft, btnY, 50, 22};
    if (UIWidget::Button(saveBtn, "Save")) {
        SaveGraph();
    }
    btnLeft += 54;

    Vector2 mp = GetMousePosition();
    bool canStop = !varValueSnapshot.empty();

    Rectangle selfBtn = {btnLeft, btnY, 48, 22};
    bool selfHovered = CheckCollisionPointRec(mp, selfBtn);
    Color selfColor = (showSelfActorDetails || selfHovered) ? theme.accent : theme.buttonBg;
    DrawRectangleRec(selfBtn, selfColor);
    DrawText("Self", (int)selfBtn.x + 7, (int)selfBtn.y + 4, 13, WHITE);

    float btnRight = bounds.x + bounds.width - 4;

    Rectangle closeBtn = {btnRight - 22, btnY, 22, 22};
    if (UIWidget::Button(closeBtn, "X")) {
        if (isDirty) SaveGraph();
        Close();
        return;
    }
    btnRight -= 26;

    Rectangle stopBtn = {btnRight - 48, btnY, 48, 22};
    if (canStop) {
        bool stopHovered = CheckCollisionPointRec(mp, stopBtn);
        DrawRectangleRec(stopBtn, stopHovered ? theme.danger : theme.dangerBg);
        DrawText("STOP", (int)stopBtn.x + 7, (int)stopBtn.y + 4, 13, WHITE);
        btnRight -= 52;
    }

    Rectangle playBtn = {btnRight - 48, btnY, 48, 22};
    if (!canStop) {
        bool playHovered = CheckCollisionPointRec(mp, playBtn);
        DrawRectangleRec(playBtn, playHovered ? theme.success : theme.successBg);
        DrawText("PLAY", (int)playBtn.x + 7, (int)playBtn.y + 4, 13, WHITE);
        btnRight -= 52;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mp, selfBtn)) {
            showSelfActorDetails = !showSelfActorDetails;
            if (showSelfActorDetails) {
                selectedNodeId = -1;
                selectedNodeIds.clear();
                selectedVariableIndex = -1;
                selectedComponentIndex = -1;
                lastValidComponentIndex = -1;
            }
        }
        if (canStop && CheckCollisionPointRec(mp, stopBtn)) {
            for (size_t i = 0; i < graph.GetVariables().size() && i < varValueSnapshot.size(); i++) {
                graph.GetVariables()[i].value = varValueSnapshot[i];
            }
            varValueSnapshot.clear();
        }
        if (!canStop && CheckCollisionPointRec(mp, playBtn)) {
            varValueSnapshot.clear();
            for (const Variable& v : graph.GetVariables()) {
                varValueSnapshot.push_back(v.value);
            }
            SaveGraph();
            GraphExecutor::Run(graph, "Event_BeginPlay");
        }
    }
}

void StargraphEditorPanel::Render() {
    if (!isOpen) return;

    DrawRectangleRec(bounds, UITheme::Get().windowBg);

    RenderToolbar();
    RenderComponentOutliner({bounds.x, bounds.y + toolbarHeight, leftPanelWidth, componentOutlinerHeight});
    RenderLeftPanel();
    RenderGraph();
    RenderNodeContextMenu();
    RenderRightPanel();
}