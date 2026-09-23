#include "StargraphEditorPanel.h"
#include "Core/InputKeyNames.h"
#include "Core/InputMappingRegistry.h"
#include "Core/NamedListRegistry.h"
#include "Core/SaveFileRegistry.h"
#include "Editor/UITheme.h"
#include "Editor/UIWidgets.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/TreeListElement.h"
#include "Core/Widgets/VerticalBoxElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "raygui.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>


static std::vector<std::string> GetParameterlessCustomEventNames(Stargraph& graph) {
    std::vector<std::string> names;
    for (Node* node : graph.GetNodesOfType("Custom_Event")) {
        if (node->outputs.size() == 1) names.push_back(node->title);
    }
    return names;
}

static std::vector<std::string> GetInputComponentNames(Stargraph& graph) {
    std::vector<std::string> names;
    for (const ComponentDecl& comp : graph.GetComponents()) {
        if (comp.componentType == "InputComponent") names.push_back(comp.name);
    }
    return names;
}

static std::vector<std::string> GetProjectInputMappingNames() {
    return NamedListRegistry::Get().GetList(InputMappingRegistry::Category());
}

void StargraphEditorPanel::BuildLeftPanelChrome() {
    leftChrome = new Widget();
    leftRoot = new VerticalBoxElement();
    leftRoot->scrollEnabled = true;
    leftRoot->childSpacing = 4.0f;
    leftChrome->SetRoot(leftRoot);

    auto makeHeader = [](const char* label) {
        auto* text = new TextElement();
        text->text = label;
        text->fontSize = 12.0f;
        text->anchor = { 0.0f, 0.5f };
        text->pivot = { 0.0f, 0.5f };
        text->offset = { 8.0f, 0.0f };
        text->size = { 0.0f, 18.0f };
        return text;
    };

    auto makeButton = [](const char* label, std::function<void()> onClick) {
        auto* btn = new ButtonElement();
        btn->size = { 0.0f, 18.0f };
        btn->onActivated = onClick;
        auto* text = new TextElement();
        text->text = label;
        text->fontSize = 11.0f;
        text->anchor = { 0.5f, 0.5f };
        text->pivot = { 0.5f, 0.5f };
        btn->AddChild(text);
        return btn;
    };

    auto makeList = [](const char* dragKind, bool allowExternal) {
        auto* list = new TreeListElement();
        list->dragKind = dragKind;
        list->allowReorder = true;
        list->allowReparent = false;
        list->allowExternalDrag = allowExternal;
        list->allowRename = true;
        list->rowHeight = 18.0f;
        list->scrollEnabled = false;
        return list;
    };

    leftRoot->AddChild(makeHeader("Graphs"));

    graphNameText = new TextElement();
    graphNameText->fontSize = 12.0f;
    graphNameText->anchor = { 0.0f, 0.5f };
    graphNameText->pivot = { 0.0f, 0.5f };
    graphNameText->offset = { 16.0f, 0.0f };
    graphNameText->size = { 0.0f, 18.0f };
    leftRoot->AddChild(graphNameText);

    leftRoot->AddChild(makeHeader("Functions"));

    funcList = makeList("StargraphFunction", true);
    funcList->onEntrySelected = [this](const std::string& id) {
        int index = atoi(id.c_str());
        selectedFunctionIndex = index;
        lastValidFunctionIndex = index;
        ClearOtherLeftSelections(0);

        float now = (float)GetTime();
        if (lastFunctionClickIndex == index && (now - lastFunctionClickTime) < 0.3f) {
            OpenFunctionGraph(index);
        }
        lastFunctionClickTime = now;
        lastFunctionClickIndex = index;
    };
    funcList->onRenameRequested = [this](const std::string& id, const std::string& newName) {
        int index = atoi(id.c_str());
        if (index >= 0 && index < (int)graph.GetFunctions().size()) {
            graph.RenameFunction(index, newName);
            isDirty = true;
        }
    };
    funcList->onDeleteRequested = [this](const std::string& id) {
        RemoveFunctionAt(atoi(id.c_str()));
    };
    funcList->onEntryRightClicked = [this](const std::string& id) {
        leftContextKind = 0;
        leftContextIndex = atoi(id.c_str());
        leftContextMenu.Open(GetMousePosition(), 110.0f, { { "Rename" }, { "Delete" } });
    };
    funcList->onReorderOrReparent = [this](const std::string& draggedId, const std::string& targetId, TreeDropPosition pos) {
        MoveInList(graph.GetFunctions(), atoi(draggedId.c_str()), atoi(targetId.c_str()), pos);
        isDirty = true;
    };
    leftRoot->AddChild(funcList);

    leftRoot->AddChild(makeButton("+ Function", [this]() {
        graph.AddFunction("NewFunction");
        int index = (int)graph.GetFunctions().size() - 1;
        selectedFunctionIndex = index;
        lastValidFunctionIndex = index;
        ClearOtherLeftSelections(0);
        isDirty = true;
        pendingRenameList = 0;
        pendingRenameIndex = index;
    }));

    leftRoot->AddChild(makeHeader("Variables"));

    varList = makeList("StargraphVariable", true);
    varList->onEntrySelected = [this](const std::string& id) {
        int index = atoi(id.c_str());
        selectedVariableIndex = index;
        lastValidVariableIndex = index;
        ClearOtherLeftSelections(1);
    };
    varList->onRenameRequested = [this](const std::string& id, const std::string& newName) {
        int index = atoi(id.c_str());
        std::vector<Variable>& vars = graph.GetVariables();
        if (index >= 0 && index < (int)vars.size()) {
            vars[index].name = newName;
            isDirty = true;
        }
    };
    varList->onDeleteRequested = [this](const std::string& id) {
        int index = atoi(id.c_str());
        std::vector<Variable>& vars = graph.GetVariables();
        if (index >= 0 && index < (int)vars.size()) {
            vars.erase(vars.begin() + index);
            selectedVariableIndex = -1;
            if (lastValidVariableIndex == index) lastValidVariableIndex = -1;
            isDirty = true;
        }
    };
    varList->onEntryRightClicked = [this](const std::string& id) {
        leftContextKind = 1;
        leftContextIndex = atoi(id.c_str());
        leftContextMenu.Open(GetMousePosition(), 110.0f, { { "Rename" }, { "Delete" } });
    };
    varList->onReorderOrReparent = [this](const std::string& draggedId, const std::string& targetId, TreeDropPosition pos) {
        MoveInList(graph.GetVariables(), atoi(draggedId.c_str()), atoi(targetId.c_str()), pos);
        isDirty = true;
    };
    leftRoot->AddChild(varList);

    leftRoot->AddChild(makeButton("+ Variable", [this]() {
        Variable v;
        v.name = "NewVar";
        v.type = PinType::Boolean;
        graph.AddVariable(v);
        int index = (int)graph.GetVariables().size() - 1;
        selectedVariableIndex = index;
        lastValidVariableIndex = index;
        ClearOtherLeftSelections(1);
        isDirty = true;
        pendingRenameList = 1;
        pendingRenameIndex = index;
    }));

    localVarSection = new VerticalBoxElement();
    localVarSection->childSpacing = 4.0f;
    localVarSection->backgroundColor = { 0, 0, 0, 0 };

    localVarHeader = makeHeader("Local Variables");
    localVarSection->AddChild(localVarHeader);

    localVarList = makeList("StargraphLocalVariable", true);
    localVarList->onEntrySelected = [this](const std::string& id) {
        int index = atoi(id.c_str());
        selectedLocalVariableIndex = index;
        lastValidLocalVariableIndex = index;
        ClearOtherLeftSelections(2);
    };
    localVarList->onRenameRequested = [this](const std::string& id, const std::string& newName) {
        int index = atoi(id.c_str());
        std::vector<Variable>& localVars = ActiveGraph().GetVariables();
        if (index >= 0 && index < (int)localVars.size()) {
            localVars[index].name = newName;
            isDirty = true;
        }
    };
    localVarList->onDeleteRequested = [this](const std::string& id) {
        int index = atoi(id.c_str());
        std::vector<Variable>& localVars = ActiveGraph().GetVariables();
        if (index >= 0 && index < (int)localVars.size()) {
            localVars.erase(localVars.begin() + index);
            selectedLocalVariableIndex = -1;
            if (lastValidLocalVariableIndex == index) lastValidLocalVariableIndex = -1;
            isDirty = true;
        }
    };
    localVarList->onEntryRightClicked = [this](const std::string& id) {
        leftContextKind = 2;
        leftContextIndex = atoi(id.c_str());
        leftContextMenu.Open(GetMousePosition(), 110.0f, { { "Rename" }, { "Delete" } });
    };
    localVarList->onReorderOrReparent = [this](const std::string& draggedId, const std::string& targetId, TreeDropPosition pos) {
        MoveInList(ActiveGraph().GetVariables(), atoi(draggedId.c_str()), atoi(targetId.c_str()), pos);
        isDirty = true;
    };
    localVarSection->AddChild(localVarList);

    localVarSection->AddChild(makeButton("+ Local Variable", [this]() {
        Variable v;
        v.name = "NewLocalVar";
        v.type = PinType::Boolean;
        ActiveGraph().AddVariable(v);
        int index = (int)ActiveGraph().GetVariables().size() - 1;
        selectedLocalVariableIndex = index;
        lastValidLocalVariableIndex = index;
        ClearOtherLeftSelections(2);
        isDirty = true;
        pendingRenameList = 2;
        pendingRenameIndex = index;
    }));

    leftRoot->AddChild(localVarSection);
}

void StargraphEditorPanel::ClearOtherLeftSelections(int keepKind) {
    if (keepKind != 0) { selectedFunctionIndex = -1; lastValidFunctionIndex = -1; }
    if (keepKind != 1) { selectedVariableIndex = -1; lastValidVariableIndex = -1; }
    if (keepKind != 2) { selectedLocalVariableIndex = -1; lastValidLocalVariableIndex = -1; }
    if (keepKind != 3) { selectedComponentIndex = -1; lastValidComponentIndex = -1; }
    selectedNodeId = -1;
    selectedNodeIds.clear();
}

void StargraphEditorPanel::RemoveFunctionAt(int index) {
    if (index < 0 || index >= (int)graph.GetFunctions().size()) return;
    graph.RemoveFunction(index);
    selectedFunctionIndex = -1;
    if (lastValidFunctionIndex == index) lastValidFunctionIndex = -1;
    if (openFunctionIndex == index) CloseFunctionGraph();
    else if (openFunctionIndex > index) openFunctionIndex--;
    isDirty = true;
}

void StargraphEditorPanel::RenderLeftPanel(Rectangle panel) {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(panel);
    leftPanelBounds = panel;

    if (!leftChrome) BuildLeftPanelChrome();

    auto styleList = [&](TreeListElement* list, int count) {
        list->backgroundColor = { 0, 0, 0, 0 };
        list->rowColor = theme.textPrimary;
        list->rowAltColor = { 0, 0, 0, 20 };
        list->rowHoveredColor = theme.surfaceHover;
        list->rowCurrentColor = theme.surfaceSelected;
        list->rowCurrentTextColor = theme.textPrimary;
        list->dropLineColor = theme.accent;
        list->size = { 0.0f, (float)count * 19.0f + 2.0f };
    };

    graphNameText->text = graph.assetName;
    graphNameText->color = theme.textPrimary;

    std::vector<FunctionDef>& funcs = graph.GetFunctions();
    std::vector<TreeListElement::Entry> funcEntries;
    funcEntries.reserve(funcs.size());
    for (size_t i = 0; i < funcs.size(); i++) {
        TreeListElement::Entry entry;
        entry.id = std::to_string(i);
        entry.label = funcs[i].name;
        entry.isCurrent = (selectedFunctionIndex == (int)i);
        funcEntries.push_back(entry);
    }
    funcList->SetEntries(funcEntries);
    styleList(funcList, (int)funcs.size());

    std::vector<Variable>& vars = graph.GetVariables();
    std::vector<TreeListElement::Entry> varEntries;
    varEntries.reserve(vars.size());
    for (size_t i = 0; i < vars.size(); i++) {
        TreeListElement::Entry entry;
        entry.id = std::to_string(i);
        entry.label = vars[i].name + (vars[i].isArray ? " []" : "");
        entry.iconColor = GetPinColor(vars[i].type);
        entry.isCurrent = (selectedVariableIndex == (int)i);
        varEntries.push_back(entry);
    }
    varList->SetEntries(varEntries);
    styleList(varList, (int)vars.size());

    bool hasOpenFunction = openFunctionIndex >= 0 && openFunctionIndex < (int)graph.GetFunctions().size();
    localVarSection->visible = hasOpenFunction;
    if (hasOpenFunction) {
        localVarHeader->text = "Local Variables (" + graph.GetFunctions()[openFunctionIndex].name + ")";
        localVarHeader->color = theme.textSecondary;

        std::vector<Variable>& localVars = ActiveGraph().GetVariables();
        std::vector<TreeListElement::Entry> localEntries;
        localEntries.reserve(localVars.size());
        for (size_t i = 0; i < localVars.size(); i++) {
            TreeListElement::Entry entry;
            entry.id = std::to_string(i);
            entry.label = localVars[i].name + (localVars[i].isArray ? " []" : "");
            entry.iconColor = GetPinColor(localVars[i].type);
            entry.isCurrent = (selectedLocalVariableIndex == (int)i);
            localEntries.push_back(entry);
        }
        localVarList->SetEntries(localEntries);
        styleList(localVarList, (int)localVars.size());
        localVarSection->size = { 0.0f, (float)localVars.size() * 19.0f + 50.0f };
    } else {
        localVarSection->size = { 0.0f, 0.0f };
    }

    if (pendingRenameIndex >= 0) {
        std::string id = std::to_string(pendingRenameIndex);
        if (pendingRenameList == 0) funcList->BeginRename(id);
        else if (pendingRenameList == 1) varList->BeginRename(id);
        else if (pendingRenameList == 2) localVarList->BeginRename(id);
        pendingRenameIndex = -1;
        pendingRenameList = -1;
    }

    leftRoot->backgroundColor = { 0, 0, 0, 0 };
    leftRoot->size = { panel.width, panel.height };

    float dt = GetFrameTime();
    Rectangle inner = { panel.x + 4, panel.y + 8, panel.width - 8, panel.height - 12 };
    leftChrome->Tick(dt, inner, 1.0f);
    WidgetInput::ProcessInput(leftChrome, "Editor", dt, GetMousePosition(), 1.0f);
    leftChrome->Draw(inner, 1.0f);
}

void StargraphEditorPanel::RenderKeyPickerPopup() {
    const UIPalette& theme = UITheme::Get();

    Pin* targetPin = nullptr;
    for (Node& node : const_cast<std::deque<Node>&>(ActiveGraph().GetNodes())) {
        for (Pin& pin : node.inputs) {
            if (pin.id == keyPickerPinId) { targetPin = &pin; break; }
        }
        if (targetPin) break;
    }
    if (!targetPin) { keyPickerOpen = false; keyPickerPinId = 0; return; }

    std::vector<std::string> allKeys;
    switch (keyPickerKind) {
        case PinPickerKind::FunctionName:  allKeys = GetParameterlessCustomEventNames(graph); break;
        case PinPickerKind::ComponentRef:  allKeys = GetInputComponentNames(graph); break;
        case PinPickerKind::MappingName:   allKeys = GetProjectInputMappingNames(); break;
        default:                           allKeys = GetInputKeyNames(); break;
    }

    std::vector<int> filtered;
    std::string searchLower = keyPickerSearch;
    for (char& c : searchLower) c = (char)tolower(c);
    for (size_t i = 0; i < allKeys.size(); i++) {
        std::string display;
        for (char c : allKeys[i]) {
            display += (c == '_') ? ' ' : (char)tolower(c);
        }
        if (searchLower.empty() || display.find(searchLower) != std::string::npos) {
            filtered.push_back((int)i);
        }
    }

    float menuW = keyPickerWidth;
    float itemH = 20.0f;
    float searchH = 26.0f;
    float maxVisibleH = 220.0f;
    float listH = std::min((float)filtered.size() * itemH + 4.0f, maxVisibleH);
    float menuH = searchH + listH;

    float popupX = keyPickerPos.x;
    float popupY = keyPickerPos.y;
    if (popupX + menuW > graphArea.x + graphArea.width) popupX = graphArea.x + graphArea.width - menuW;
    if (popupY + menuH > graphArea.y + graphArea.height) popupY = graphArea.y + graphArea.height - menuH;
    if (popupX < graphArea.x) popupX = graphArea.x;
    if (popupY < graphArea.y) popupY = graphArea.y;

    Rectangle popup = {popupX, popupY, menuW, menuH};
    Vector2 mp = GetMousePosition();

    if (keyPickerJustOpened) {
        keyPickerJustOpened = false;
    } else {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, popup)) {
            keyPickerOpen = false;
            keyPickerPinId = 0;
            return;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            keyPickerOpen = false;
            keyPickerPinId = 0;
            return;
        }
    }

    UIWidget::Panel(popup);

    Rectangle searchRect = {popup.x + 4, popup.y + 4, popup.width - 8, searchH - 6};
    GuiTextBox(searchRect, keyPickerSearch, sizeof(keyPickerSearch), true);

    Rectangle listArea = {popup.x, popup.y + searchH, popup.width, listH};
    if (CheckCollisionPointRec(mp, listArea)) {
        float wheel = GetMouseWheelMove();
        keyPickerScroll -= wheel * itemH * 2;
    }
    float maxScroll = (float)filtered.size() * itemH + 4.0f - listH;
    if (maxScroll < 0) maxScroll = 0;
    if (keyPickerScroll < 0) keyPickerScroll = 0;
    if (keyPickerScroll > maxScroll) keyPickerScroll = maxScroll;

    float y = popup.y + searchH + 4 - keyPickerScroll;
    BeginScissorMode((int)listArea.x, (int)listArea.y, (int)listArea.width, (int)listArea.height);
    for (int idx : filtered) {
        Rectangle btn = {popup.x + 4, y, popup.width - 8, itemH - 2};
        bool hover = CheckCollisionPointRec(mp, btn);
        if (hover) DrawRectangleRec(btn, theme.surfaceHover);

        std::string display;
        for (char c : allKeys[idx]) {
            if (c == '_') display += ' ';
            else display += c;
        }
        DrawText(display.c_str(), (int)btn.x + 6, (int)btn.y + 4, 11, theme.textPrimary);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            targetPin->defaultValue = allKeys[idx];
            isDirty = true;
            keyPickerOpen = false;
            keyPickerPinId = 0;
            break;
        }
        y += itemH;
    }
    EndScissorMode();

    if ((float)filtered.size() * itemH + 4.0f > listH) {
        float scrollRatio = (maxScroll > 0) ? keyPickerScroll / maxScroll : 0;
        float barH = (listH / ((float)filtered.size() * itemH + 4.0f)) * listH;
        float barY = listArea.y + scrollRatio * (listH - barH);
        DrawRectangleRec({listArea.x + listArea.width - 4, barY, 3, barH}, {100, 100, 120, 200});
    }
}
void StargraphEditorPanel::RenderLeftPanelPopups() {
    if (!leftContextMenu.IsOpen()) return;

    int choice = -1;
    if (!leftContextMenu.Render(choice)) return;

    if (choice == 0) {
        std::string id = std::to_string(leftContextIndex);
        if (leftContextKind == 0) funcList->BeginRename(id);
        else if (leftContextKind == 1) varList->BeginRename(id);
        else if (leftContextKind == 2) localVarList->BeginRename(id);
    } else if (choice == 1) {
        if (leftContextKind == 0) {
            RemoveFunctionAt(leftContextIndex);
        } else if (leftContextKind == 1) {
            std::vector<Variable>& vars = graph.GetVariables();
            if (leftContextIndex >= 0 && leftContextIndex < (int)vars.size()) {
                vars.erase(vars.begin() + leftContextIndex);
                selectedVariableIndex = -1;
                if (lastValidVariableIndex == leftContextIndex) lastValidVariableIndex = -1;
                isDirty = true;
            }
        } else if (leftContextKind == 2) {
            std::vector<Variable>& localVars = ActiveGraph().GetVariables();
            if (leftContextIndex >= 0 && leftContextIndex < (int)localVars.size()) {
                localVars.erase(localVars.begin() + leftContextIndex);
                selectedLocalVariableIndex = -1;
                if (lastValidLocalVariableIndex == leftContextIndex) lastValidLocalVariableIndex = -1;
                isDirty = true;
            }
        }
    }

    leftContextKind = -1;
    leftContextIndex = -1;
}
