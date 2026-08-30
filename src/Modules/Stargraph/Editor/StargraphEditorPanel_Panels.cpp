#include "StargraphEditorPanel.h"
#include "Editor/PropertyFields.h"
#include "Modules/Stargraph/InputKeyNames.h"
#include "Core/InputMappingRegistry.h"
#include "Core/NamedListRegistry.h"
#include "Core/SaveFileRegistry.h"
#include "Editor/UITheme.h"
#include "Editor/UIWidgets.h"
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

void StargraphEditorPanel::RenderLeftPanel(Rectangle panel) {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(panel);

    Vector2 mousePos = GetMousePosition();
    float y = panel.y + 8;

    auto sectionHeader = [&](const char* label) {
        DrawText(label, (int)panel.x + 8, (int)y, 12, theme.textSecondary);
        y += 18;
        DrawLine((int)panel.x + 4, (int)y, (int)(panel.x + panel.width - 4), (int)y, theme.panelBorder);
        y += 6;
    };

    sectionHeader("Graphs");
    DrawRectangleRec({panel.x + 4, y, panel.width - 8, 18}, theme.fieldBg);
    DrawText(graph.assetName.c_str(), (int)panel.x + 20, (int)y + 2, 12, theme.textPrimary);
    y += 22;

    bool funcContextMenuWasOpenThisFrame = funcContextMenuOpen;
    bool wasAddingFunctionThisFrame = addingFunction;
    bool varContextMenuWasOpenThisFrame = varContextMenuOpen;
    bool wasAddingVariableThisFrame = addingVariable;
    bool localVarContextMenuWasOpenThisFrame = localVarContextMenuOpen;
    bool wasAddingLocalVariableThisFrame = addingLocalVariable;

    y += 8;
    sectionHeader("Functions");
    bool leftClickConsumedByFunction = false;

    std::vector<FunctionDef>& funcs = graph.GetFunctions();
    for (size_t i = 0; i < funcs.size(); i++) {
        Rectangle row = {panel.x + 4, y, panel.width - 8, 18};
        bool isSelected = (selectedFunctionIndex == (int)i);

        DrawRectangleRec(row, isSelected ? theme.surfaceSelected : theme.fieldBg);

        if (renamingFunctionIndex == (int)i) {
            std::string nameOut = funcs[i].name;
            if (PropertyField::StringBox({panel.x + 12, y, panel.width - 18, 18}, funcNameBuffer,
                                          sizeof(funcNameBuffer), 1, functionNameActiveField, nameOut)) {
                if (!nameOut.empty()) graph.RenameFunction(i, nameOut);
                renamingFunctionIndex = -1;
                functionNameActiveField = 0;
                isDirty = true;
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                renamingFunctionIndex = -1;
                functionNameActiveField = 0;
            }
        } else {
            DrawText(funcs[i].name.c_str(), (int)panel.x + 12, (int)y + 2, 12, theme.textPrimary);

            if (CheckCollisionPointRec(mousePos, row)) {
                if (!funcContextMenuOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    leftClickConsumedByFunction = true;
                    selectedFunctionIndex = (int)i;
                    lastValidFunctionIndex = (int)i;
                    selectedVariableIndex = -1;
                    lastValidVariableIndex = -1;
                    selectedComponentIndex = -1;
                    lastValidComponentIndex = -1;
                    selectedLocalVariableIndex = -1;
                    lastValidLocalVariableIndex = -1;
                    selectedNodeId = -1;
                    selectedNodeIds.clear();

                    float currentTime = (float)GetTime();
                    if (lastFunctionClickIndex == (int)i && (currentTime - lastFunctionClickTime) < 0.3f) {
                        OpenFunctionGraph((int)i);
                    }
                    lastFunctionClickTime = currentTime;
                    lastFunctionClickIndex = (int)i;
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    leftClickConsumedByFunction = true;
                    selectedFunctionIndex = (int)i;
                    funcContextMenuOpen = true;
                    funcContextMenuIndex = (int)i;
                    funcContextMenuAnchorY = y;
                }
            }
        }

        if (isSelected && IsKeyPressed(KEY_DELETE)) {
            graph.RemoveFunction(i);
            selectedFunctionIndex = -1;
            if (lastValidFunctionIndex == (int)i) lastValidFunctionIndex = -1;
            if (openFunctionIndex == (int)i) CloseFunctionGraph();
            else if (openFunctionIndex > (int)i) openFunctionIndex--;
            isDirty = true;
            break;
        }

        y += 22;
    }

    if (addingFunction) {
        Rectangle row = {panel.x + 4, y, panel.width - 8, 18};
        DrawRectangleRec(row, theme.fieldBg);

        std::string nameOut = funcNameBuffer;
        if (PropertyField::StringBox({panel.x + 12, y, panel.width - 18, 18}, funcNameBuffer,
                                      sizeof(funcNameBuffer), 1, functionNameActiveField, nameOut)) {
            if (!nameOut.empty()) {
                graph.AddFunction(nameOut);
                selectedFunctionIndex = (int)graph.GetFunctions().size() - 1;
                lastValidFunctionIndex = selectedFunctionIndex;
                isDirty = true;
            }
            addingFunction = false;
            functionNameActiveField = 0;
        } else if (IsKeyPressed(KEY_ESCAPE)) {
            addingFunction = false;
            functionNameActiveField = 0;
        }
        y += 22;
    }

    Rectangle addFuncBtn = {panel.x + 4, y + 4, panel.width - 8, 16};
    UIWidget::Button(addFuncBtn, "+ Function");

    if (!funcContextMenuOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, addFuncBtn)) {
        snprintf(funcNameBuffer, sizeof(funcNameBuffer), "NewFunction");
        PropertyField::ForceActivate(funcNameBuffer, sizeof(funcNameBuffer));
        addingFunction = true;
        functionNameActiveField = 1;
    }
    y += 24;

    if (funcContextMenuOpen) {
        Rectangle popup = {panel.x + panel.width, funcContextMenuAnchorY - 4, 110, 50};
        UIWidget::Panel(popup);

        Rectangle renameBtn = {popup.x + 4, popup.y + 4, popup.width - 8, 18};
        if (UIWidget::Button(renameBtn, "Rename")) {
            snprintf(funcNameBuffer, sizeof(funcNameBuffer), "%s", funcs[funcContextMenuIndex].name.c_str());
            PropertyField::ForceActivate(funcNameBuffer, sizeof(funcNameBuffer));
            renamingFunctionIndex = funcContextMenuIndex;
            functionNameActiveField = 1;
            funcContextMenuOpen = false;
        }
        if (UIWidget::Button({popup.x + 4, popup.y + 26, popup.width - 8, 18}, "Delete")) {
            graph.RemoveFunction(funcContextMenuIndex);
            selectedFunctionIndex = -1;
            if (lastValidFunctionIndex == funcContextMenuIndex) lastValidFunctionIndex = -1;
            if (openFunctionIndex == funcContextMenuIndex) CloseFunctionGraph();
            else if (openFunctionIndex > funcContextMenuIndex) openFunctionIndex--;
            funcContextMenuOpen = false;
            isDirty = true;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, popup)) {
            funcContextMenuOpen = false;
        }
    }

    if (!funcContextMenuWasOpenThisFrame && !wasAddingFunctionThisFrame && !leftClickConsumedByFunction &&
        selectedFunctionIndex != -1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        selectedFunctionIndex = -1;
    }

    y += 8;
    sectionHeader("Variables");
    bool leftClickConsumedByVariable = false;

    std::vector<Variable>& vars = graph.GetVariables();
    for (size_t i = 0; i < vars.size(); i++) {
        Rectangle row = {panel.x + 4, y, panel.width - 8, 18};
        bool isSelected = (selectedVariableIndex == (int)i);

        DrawRectangleRec(row, isSelected ? theme.surfaceSelected : theme.fieldBg);
        DrawRectangleRec({panel.x + 8, y + 4, 10, 10}, GetPinColor(vars[i].type));

        if (renamingVariableIndex == (int)i) {
            std::string nameOut = vars[i].name;
            if (PropertyField::StringBox({panel.x + 24, y, panel.width - 30, 18}, varNameBuffer,
                                          sizeof(varNameBuffer), 1, variableNameActiveField, nameOut)) {
                vars[i].name = nameOut;
                renamingVariableIndex = -1;
                variableNameActiveField = 0;
                isDirty = true;
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                renamingVariableIndex = -1;
                variableNameActiveField = 0;
            }
        } else {
            std::string label = vars[i].name + (vars[i].isArray ? " []" : "");
            DrawText(label.c_str(), (int)panel.x + 24, (int)y + 2, 12, theme.textPrimary);

            if (CheckCollisionPointRec(mousePos, row)) {
                if (!varContextMenuOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    leftClickConsumedByVariable = true;
                    selectedVariableIndex = (int)i;
                    lastValidVariableIndex = (int)i;
                    selectedFunctionIndex = -1;
                    lastValidFunctionIndex = -1;
                    selectedComponentIndex = -1;
                    lastValidComponentIndex = -1;
                    selectedLocalVariableIndex = -1;
                    lastValidLocalVariableIndex = -1;
                    selectedNodeId = -1;
                    selectedNodeIds.clear();
                    variableDragIndex = (int)i;
                    variableDragIsLocal = false;
                    variableDragActive = false;
                    variableDragPos = mousePos;
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    leftClickConsumedByVariable = true;
                    selectedVariableIndex = (int)i;
                    selectedComponentIndex = -1;
                    lastValidComponentIndex = -1;
                    varContextMenuOpen = true;
                    varContextMenuIndex = (int)i;
                    varContextMenuAnchorY = y;
                }
            }
        }

        if (variableDragIndex >= 0 && variableDragIndex < (int)vars.size() && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float dx = mousePos.x - variableDragPos.x;
            float dy = mousePos.y - variableDragPos.y;
            if (!variableDragActive && (dx*dx + dy*dy > 25)) {
                variableDragActive = true;
            }
        }

        if (isSelected && IsKeyPressed(KEY_DELETE)) {
            vars.erase(vars.begin() + i);
            selectedVariableIndex = -1;
            if (lastValidVariableIndex == (int)i) lastValidVariableIndex = -1;
            isDirty = true;
            break;
        }

        y += 22;
    }

    if (addingVariable) {
        Rectangle row = {panel.x + 4, y, panel.width - 8, 18};
        DrawRectangleRec(row, theme.fieldBg);
        DrawRectangleRec({panel.x + 8, y + 4, 10, 10}, GetPinColor(PinType::Boolean));

        std::string nameOut = varNameBuffer;
        if (PropertyField::StringBox({panel.x + 24, y, panel.width - 30, 18}, varNameBuffer,
                                      sizeof(varNameBuffer), 1, variableNameActiveField, nameOut)) {
            if (!nameOut.empty()) {
                Variable v;
                v.name = nameOut;
                v.type = PinType::Boolean;
                graph.AddVariable(v);
                selectedVariableIndex = (int)graph.GetVariables().size() - 1;
                lastValidVariableIndex = selectedVariableIndex;
                isDirty = true;
            }
            addingVariable = false;
            variableNameActiveField = 0;
        } else if (IsKeyPressed(KEY_ESCAPE)) {
            addingVariable = false;
            variableNameActiveField = 0;
        }
        y += 22;
    }

    Rectangle addBtn = {panel.x + 4, y + 4, panel.width - 8, 16};
    UIWidget::Button(addBtn, "+ Variable");
    
    if (!varContextMenuOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, addBtn)) {
        snprintf(varNameBuffer, sizeof(varNameBuffer), "NewVar");
        PropertyField::ForceActivate(varNameBuffer, sizeof(varNameBuffer));
        addingVariable = true;
        variableNameActiveField = 1;
    }
    y += 24;

    if (varContextMenuOpen) {
        Rectangle popup = {panel.x + panel.width, varContextMenuAnchorY - 4, 110, 50};
        UIWidget::Panel(popup);

        Rectangle renameBtn = {popup.x + 4, popup.y + 4, popup.width - 8, 18};
        if (UIWidget::Button(renameBtn, "Rename")) {
            snprintf(varNameBuffer, sizeof(varNameBuffer), "%s", vars[varContextMenuIndex].name.c_str());
            PropertyField::ForceActivate(varNameBuffer, sizeof(varNameBuffer));
            renamingVariableIndex = varContextMenuIndex;
            variableNameActiveField = 1;
            varContextMenuOpen = false;
        }
        if (UIWidget::Button({popup.x + 4, popup.y + 26, popup.width - 8, 18}, "Delete")) {
            vars.erase(vars.begin() + varContextMenuIndex);
            selectedVariableIndex = -1;
            if (lastValidVariableIndex == varContextMenuIndex) lastValidVariableIndex = -1;
            varContextMenuOpen = false;
            isDirty = true;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, popup)) {
            varContextMenuOpen = false;
        }
    }

    if (!varContextMenuWasOpenThisFrame && !wasAddingVariableThisFrame && !leftClickConsumedByVariable &&
        selectedVariableIndex != -1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        selectedVariableIndex = -1;
    }

    if (openFunctionIndex >= 0 && openFunctionIndex < (int)graph.GetFunctions().size()) {
        y += 8;
        sectionHeader(("Local Variables (" + graph.GetFunctions()[openFunctionIndex].name + ")").c_str());
        bool leftClickConsumedByLocalVariable = false;

        std::vector<Variable>& localVars = ActiveGraph().GetVariables();
        for (size_t i = 0; i < localVars.size(); i++) {
            Rectangle row = {panel.x + 4, y, panel.width - 8, 18};
            bool isSelected = (selectedLocalVariableIndex == (int)i);

            DrawRectangleRec(row, isSelected ? theme.surfaceSelected : theme.fieldBg);
            DrawRectangleRec({panel.x + 8, y + 4, 10, 10}, GetPinColor(localVars[i].type));

            if (renamingLocalVariableIndex == (int)i) {
                std::string nameOut = localVars[i].name;
                if (PropertyField::StringBox({panel.x + 24, y, panel.width - 30, 18}, localVarNameBuffer,
                                              sizeof(localVarNameBuffer), 1, localVariableNameActiveField, nameOut)) {
                    localVars[i].name = nameOut;
                    renamingLocalVariableIndex = -1;
                    localVariableNameActiveField = 0;
                    isDirty = true;
                } else if (IsKeyPressed(KEY_ESCAPE)) {
                    renamingLocalVariableIndex = -1;
                    localVariableNameActiveField = 0;
                }
            } else {
                std::string label = localVars[i].name + (localVars[i].isArray ? " []" : "");
                DrawText(label.c_str(), (int)panel.x + 24, (int)y + 2, 12, theme.textPrimary);

                if (CheckCollisionPointRec(mousePos, row)) {
                    if (!localVarContextMenuOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        leftClickConsumedByLocalVariable = true;
                        selectedLocalVariableIndex = (int)i;
                        lastValidLocalVariableIndex = (int)i;
                        selectedVariableIndex = -1;
                        lastValidVariableIndex = -1;
                        selectedFunctionIndex = -1;
                        lastValidFunctionIndex = -1;
                        selectedComponentIndex = -1;
                        lastValidComponentIndex = -1;
                        selectedNodeId = -1;
                        selectedNodeIds.clear();
                        variableDragIndex = (int)i;
                        variableDragIsLocal = true;
                        variableDragActive = false;
                        variableDragPos = mousePos;
                    }
                    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                        leftClickConsumedByLocalVariable = true;
                        selectedLocalVariableIndex = (int)i;
                        localVarContextMenuOpen = true;
                        localVarContextMenuIndex = (int)i;
                        localVarContextMenuAnchorY = y;
                    }
                }
            }

            if (isSelected && IsKeyPressed(KEY_DELETE)) {
                localVars.erase(localVars.begin() + i);
                selectedLocalVariableIndex = -1;
                if (lastValidLocalVariableIndex == (int)i) lastValidLocalVariableIndex = -1;
                isDirty = true;
                break;
            }

            y += 22;
        }

        if (addingLocalVariable) {
            Rectangle row = {panel.x + 4, y, panel.width - 8, 18};
            DrawRectangleRec(row, theme.fieldBg);
            DrawRectangleRec({panel.x + 8, y + 4, 10, 10}, GetPinColor(PinType::Boolean));

            std::string nameOut = localVarNameBuffer;
            if (PropertyField::StringBox({panel.x + 24, y, panel.width - 30, 18}, localVarNameBuffer,
                                          sizeof(localVarNameBuffer), 1, localVariableNameActiveField, nameOut)) {
                if (!nameOut.empty()) {
                    Variable v;
                    v.name = nameOut;
                    v.type = PinType::Boolean;
                    ActiveGraph().AddVariable(v);
                    selectedLocalVariableIndex = (int)ActiveGraph().GetVariables().size() - 1;
                    lastValidLocalVariableIndex = selectedLocalVariableIndex;
                    isDirty = true;
                }
                addingLocalVariable = false;
                localVariableNameActiveField = 0;
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                addingLocalVariable = false;
                localVariableNameActiveField = 0;
            }
            y += 22;
        }

        Rectangle addLocalBtn = {panel.x + 4, y + 4, panel.width - 8, 16};
        UIWidget::Button(addLocalBtn, "+ Local Variable");

        if (!localVarContextMenuOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, addLocalBtn)) {
            snprintf(localVarNameBuffer, sizeof(localVarNameBuffer), "NewLocalVar");
            PropertyField::ForceActivate(localVarNameBuffer, sizeof(localVarNameBuffer));
            addingLocalVariable = true;
            localVariableNameActiveField = 1;
        }
        y += 24;

        if (localVarContextMenuOpen) {
            Rectangle popup = {panel.x + panel.width, localVarContextMenuAnchorY - 4, 110, 50};
            UIWidget::Panel(popup);

            Rectangle renameBtn = {popup.x + 4, popup.y + 4, popup.width - 8, 18};
            if (UIWidget::Button(renameBtn, "Rename")) {
                snprintf(localVarNameBuffer, sizeof(localVarNameBuffer), "%s", localVars[localVarContextMenuIndex].name.c_str());
                PropertyField::ForceActivate(localVarNameBuffer, sizeof(localVarNameBuffer));
                renamingLocalVariableIndex = localVarContextMenuIndex;
                localVariableNameActiveField = 1;
                localVarContextMenuOpen = false;
            }
            if (UIWidget::Button({popup.x + 4, popup.y + 26, popup.width - 8, 18}, "Delete")) {
                localVars.erase(localVars.begin() + localVarContextMenuIndex);
                selectedLocalVariableIndex = -1;
                if (lastValidLocalVariableIndex == localVarContextMenuIndex) lastValidLocalVariableIndex = -1;
                localVarContextMenuOpen = false;
                isDirty = true;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, popup)) {
                localVarContextMenuOpen = false;
            }
        }

        if (!localVarContextMenuWasOpenThisFrame && !wasAddingLocalVariableThisFrame && !leftClickConsumedByLocalVariable &&
            selectedLocalVariableIndex != -1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            selectedLocalVariableIndex = -1;
        }
    }
}

void StargraphEditorPanel::RenderVariableArrayEditor(Rectangle panel, float& y, Variable& var, int& activeField) {
    const UIPalette& theme = UITheme::Get();
    if (varArrayBufs.size() != var.arrayValues.size()) {
        varArrayBufs.resize(var.arrayValues.size());
    }

    PropertyField::ArrayAction action;

    Rectangle headerRect = {panel.x + 8, y, panel.width - 16, PropertyField::ArrayRowHeight()};
    PropertyField::ArrayHeader(headerRect, "Elements", (int)var.arrayValues.size(), action);
    y += PropertyField::ArrayRowHeight() + 4;

    for (size_t i = 0; i < var.arrayValues.size(); i++) {
        Rectangle rowRect = {panel.x + 8, y, panel.width - 16, PropertyField::ArrayRowHeight()};
        Rectangle valueRect = PropertyField::ArrayValueRect(rowRect);
        int fieldId = 10 + (int)i * 4; // 4 id slots per element; Vec3Row needs 3

        std::string elem = var.arrayValues[i];
        bool changed = false;

        switch (var.type) {
            case PinType::Boolean: {
                bool bval = (elem == "true" || elem == "1");
                Rectangle boolRect = {valueRect.x, valueRect.y, 18, valueRect.height};
                if (PropertyField::BoolBox(boolRect, bval)) {
                    elem = bval ? "true" : "false";
                    changed = true;
                }
                DrawText(bval ? "true" : "false", (int)boolRect.x + 24, (int)boolRect.y + 2, 11, theme.textPrimary);
                break;
            }
            case PinType::Integer: {
                int ival = atoi(elem.c_str());
                if (PropertyField::IntBox(valueRect, varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]),
                                           fieldId, activeField, ival)) {
                    elem = std::to_string(ival);
                    changed = true;
                }
                break;
            }
            case PinType::Float: {
                float fval = (float)atof(elem.c_str());
                if (PropertyField::FloatBox(valueRect, varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]),
                                             fieldId, activeField, fval)) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.2f", fval);
                    elem = buf;
                    changed = true;
                }
                break;
            }
            case PinType::Vec3: {
                float vx = 0, vy = 0, vz = 0;
                sscanf(elem.c_str(), "%f,%f,%f", &vx, &vy, &vz);
                if (PropertyField::Vec3Row(valueRect, varArrayBufs[i].text[0], varArrayBufs[i].text[1],
                                            varArrayBufs[i].text[2], fieldId, activeField, vx, vy, vz)) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f", vx, vy, vz);
                    elem = buf;
                    changed = true;
                }
                break;
            }
            default: {
                std::string strVal = elem;
                if (PropertyField::StringBox(valueRect, varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]),
                                              fieldId, activeField, strVal, true, "(empty)")) {
                    elem = strVal;
                    changed = true;
                }
                break;
            }
        }

        if (changed) {
            var.arrayValues[i] = elem;
            isDirty = true;
        }

        PropertyField::ArrayRowControls(rowRect, (int)i, (int)var.arrayValues.size(), action);
        y += PropertyField::ArrayRowHeight() + 3;
    }

    if (action.addClicked) {
        std::string def = var.DefaultElementText();
        var.arrayValues.push_back(def);

        ArrayElemBuf buf;
        if (var.type == PinType::Vec3) {
            float vx = 0, vy = 0, vz = 0;
            sscanf(def.c_str(), "%f,%f,%f", &vx, &vy, &vz);
            snprintf(buf.text[0], sizeof(buf.text[0]), "%.2f", vx);
            snprintf(buf.text[1], sizeof(buf.text[1]), "%.2f", vy);
            snprintf(buf.text[2], sizeof(buf.text[2]), "%.2f", vz);
        } else {
            snprintf(buf.text[0], sizeof(buf.text[0]), "%s", def.c_str());
        }
        varArrayBufs.push_back(buf);
        isDirty = true;
    } else if (action.removeIndex >= 0 && action.removeIndex < (int)var.arrayValues.size()) {
        var.arrayValues.erase(var.arrayValues.begin() + action.removeIndex);
        if (action.removeIndex < (int)varArrayBufs.size())
            varArrayBufs.erase(varArrayBufs.begin() + action.removeIndex);
        isDirty = true;
        activeField = 0;
    } else if (action.moveUpIndex > 0 && action.moveUpIndex < (int)var.arrayValues.size()) {
        std::swap(var.arrayValues[action.moveUpIndex], var.arrayValues[action.moveUpIndex - 1]);
        if (action.moveUpIndex < (int)varArrayBufs.size())
            std::swap(varArrayBufs[action.moveUpIndex], varArrayBufs[action.moveUpIndex - 1]);
        isDirty = true;
        activeField = 0;
    } else if (action.moveDownIndex >= 0 && action.moveDownIndex + 1 < (int)var.arrayValues.size()) {
        std::swap(var.arrayValues[action.moveDownIndex], var.arrayValues[action.moveDownIndex + 1]);
        if (action.moveDownIndex + 1 < (int)varArrayBufs.size())
            std::swap(varArrayBufs[action.moveDownIndex], varArrayBufs[action.moveDownIndex + 1]);
        isDirty = true;
        activeField = 0;
    }
}

void StargraphEditorPanel::RenderVariableDetailsBody(Rectangle panel, float y, Variable& var, int detailIndex, bool isLocal) {
    const UIPalette& theme = UITheme::Get();

    DrawText("Name", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
    y += 14;

    static char varNameEditBuf[128] = "";
    static char varValBuffer[128] = "";
    static char varIntBuf[32] = "";
    static char varFloatBuf[32] = "";
    static char varVecBuf[3][32] = {"", "", ""};
    static int lastDetailIndex = -1;
    static bool lastDetailWasLocal = false;
    static int activeField = 0;

    auto refreshValueBuffers = [&]() {
        switch (var.type) {
            case PinType::Integer:
                snprintf(varIntBuf, sizeof(varIntBuf), "%d", atoi(var.value.c_str()));
                break;
            case PinType::Float:
                snprintf(varFloatBuf, sizeof(varFloatBuf), "%.2f", atof(var.value.c_str()));
                break;
            case PinType::Vec3: {
                float vx = 0, vy = 0, vz = 0;
                sscanf(var.value.c_str(), "%f,%f,%f", &vx, &vy, &vz);
                snprintf(varVecBuf[0], 32, "%.2f", vx);
                snprintf(varVecBuf[1], 32, "%.2f", vy);
                snprintf(varVecBuf[2], 32, "%.2f", vz);
                break;
            }
            default:
                snprintf(varValBuffer, sizeof(varValBuffer), "%s", var.value.c_str());
                break;
        }

        varArrayBufs.resize(var.arrayValues.size());
        for (size_t i = 0; i < var.arrayValues.size(); i++) {
            const std::string& elem = var.arrayValues[i];
            switch (var.type) {
                case PinType::Vec3: {
                    float vx = 0, vy = 0, vz = 0;
                    sscanf(elem.c_str(), "%f,%f,%f", &vx, &vy, &vz);
                    snprintf(varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]), "%.2f", vx);
                    snprintf(varArrayBufs[i].text[1], sizeof(varArrayBufs[i].text[1]), "%.2f", vy);
                    snprintf(varArrayBufs[i].text[2], sizeof(varArrayBufs[i].text[2]), "%.2f", vz);
                    break;
                }
                case PinType::Integer:
                    snprintf(varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]), "%d", atoi(elem.c_str()));
                    break;
                case PinType::Float:
                    snprintf(varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]), "%.2f", atof(elem.c_str()));
                    break;
                default:
                    snprintf(varArrayBufs[i].text[0], sizeof(varArrayBufs[i].text[0]), "%s", elem.c_str());
                    break;
            }
        }
    };

    if (lastDetailIndex != detailIndex || lastDetailWasLocal != isLocal) {
        snprintf(varNameEditBuf, sizeof(varNameEditBuf), "%s", var.name.c_str());
        refreshValueBuffers();
        lastDetailIndex = detailIndex;
        lastDetailWasLocal = isLocal;
        activeField = 0;
        savePicker.Close();
    }

    Rectangle nameRect = {panel.x + 8, y, panel.width - 16, 18};
    std::string varName = var.name;
    if (PropertyField::StringBox(nameRect, varNameEditBuf, sizeof(varNameEditBuf), 1, activeField, varName)) {
        var.name = varName;
        isDirty = true;
    }
    y += 24;

    DrawText("Type", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
    y += 14;

    static const char* typeNames[] = {"None","Exec","Boolean","Integer","Float","String","Vec3","Object","Wildcard"};
    int typeIndex = (int)var.type;
    if (typeIndex < 0 || typeIndex > 8) typeIndex = 2;

    Rectangle typeRect = {panel.x + 8, y, panel.width - 16, 18};
    PropertyField::Style typeStyle;
    DrawRectangleRec(typeRect, typeStyle.box);
    DrawRectangleLinesEx(typeRect, 1, typeStyle.border);
    DrawText(typeNames[typeIndex], (int)typeRect.x + 4, (int)typeRect.y + 3, 11, typeStyle.text);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), typeRect) && !savePicker.IsOpen()) {
        int next = typeIndex + 1;
        if (next < 2 || next > 7) next = 2;
        var.type = (PinType)next;
        switch (var.type) {
            case PinType::Boolean: var.value = "false"; break;
            case PinType::Integer: var.value = "0"; break;
            case PinType::Float:   var.value = "0.00"; break;
            case PinType::Vec3:    var.value = "0.00,0.00,0.00"; break;
            default:               var.value = ""; break;
        }
        var.arrayValues.clear();
        varArrayBufs.clear();
        refreshValueBuffers();
        isDirty = true;
        activeField = 0;
    }
    y += 24;

    if (var.type == PinType::Object) {
        DrawText("Kind", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
        y += 14;

        Rectangle kindRect = {panel.x + 8, y, panel.width - 16, 18};
        DrawRectangleRec(kindRect, {35, 35, 40, 255});
        DrawRectangleLinesEx(kindRect, 1, {60, 60, 70, 255});
        std::string kindLabel = var.objectKind;
        if (kindLabel.empty()) {
            kindLabel = "Any";
        } else {
            size_t slash = kindLabel.find_last_of("/\\");
            if (slash != std::string::npos) kindLabel = kindLabel.substr(slash + 1);
            size_t dot = kindLabel.find_last_of('.');
            if (dot != std::string::npos) kindLabel = kindLabel.substr(0, dot);
        }
        DrawText(kindLabel.c_str(), (int)kindRect.x + 4, (int)kindRect.y + 3, 11, theme.textPrimary);

        Rectangle clearRect = {kindRect.x + kindRect.width - 20, kindRect.y, 20, 18};
        if (!var.objectKind.empty()) {
            DrawText("x", (int)clearRect.x + 6, (int)clearRect.y + 3, 11, theme.textSecondary);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), clearRect)) {
                var.objectKind = "";
                isDirty = true;
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), kindRect) &&
            !CheckCollisionPointRec(GetMousePosition(), clearRect) && !savePicker.IsOpen()) {
            actorClassPickerVarIndex = detailIndex;
            actorClassPickerPinId = 0;
            actorClassPicker.Open({kindRect.x, kindRect.y + kindRect.height}, ".staractor");
        }
        if (actorClassPickerVarIndex == detailIndex) {
            std::string pickedClass;
            if (actorClassPicker.Render(pickedClass)) {
                var.objectKind = pickedClass;
                isDirty = true;
                actorClassPickerVarIndex = -1;
            }
        }
        y += 26;
    }

    DrawText("Array", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    {
        Rectangle arrayCheckRect = {panel.x + 8, y, 18, 18};
        bool isArr = var.isArray;
        if (PropertyField::BoolBox(arrayCheckRect, isArr)) {
            var.isArray = isArr;
            if (var.isArray) {
                var.arrayValues.clear();
                var.arrayValues.push_back(var.value.empty() ? var.DefaultElementText() : var.value);
            } else {
                var.value = var.arrayValues.empty() ? var.DefaultElementText() : var.arrayValues.front();
                var.arrayValues.clear();
            }
            varArrayBufs.clear();
            refreshValueBuffers();
            isDirty = true;
            activeField = 0;
        }
        DrawText(isArr ? "true" : "false", (int)arrayCheckRect.x + 24, (int)arrayCheckRect.y + 2, 12, theme.textPrimary);
    }
    y += 24;

    DrawText("Default Value", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
    y += 14;

    if (var.isArray) {
        RenderVariableArrayEditor(panel, y, var, activeField);
    } else if (var.type == PinType::Boolean) {
        bool bval = (var.value == "true" || var.value == "1");
        Rectangle boolRect = {panel.x + 8, y, 18, 18};
        if (PropertyField::BoolBox(boolRect, bval)) {
            var.value = bval ? "true" : "false";
            isDirty = true;
        }
        DrawText(bval ? "true" : "false", (int)boolRect.x + 24, (int)boolRect.y + 2, 12, theme.textPrimary);
    } else if (var.type == PinType::Integer) {
        Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
        int ival = atoi(var.value.c_str());
        if (PropertyField::IntBox(r, varIntBuf, sizeof(varIntBuf), 2, activeField, ival)) {
            var.value = std::to_string(ival);
            isDirty = true;
        }
    } else if (var.type == PinType::Float) {
        Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
        float fval = (float)atof(var.value.c_str());
        if (PropertyField::FloatBox(r, varFloatBuf, sizeof(varFloatBuf), 2, activeField, fval)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f", fval);
            var.value = buf;
            isDirty = true;
        }
    } else if (var.type == PinType::Vec3) {
        Rectangle row = {panel.x + 8, y, panel.width - 16, 18};
        float vx = 0, vy = 0, vz = 0;
        sscanf(var.value.c_str(), "%f,%f,%f", &vx, &vy, &vz);
        if (PropertyField::Vec3Row(row, varVecBuf[0], varVecBuf[1], varVecBuf[2], 2, activeField,
                                    vx, vy, vz)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f", vx, vy, vz);
            var.value = buf;
            isDirty = true;
        }
    } else {
        Rectangle valRect = {panel.x + 8, y, panel.width - 16, 18};
        std::string varValue = var.value;
        if (PropertyField::StringBox(valRect, varValBuffer, sizeof(varValBuffer), 2, activeField, varValue,
                                      true, "(empty)")) {
            var.value = varValue;
            isDirty = true;
        }
    }
    y += 26;

    if (isLocal) return;

    DrawText("Replication", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
    y += 14;

    static const char* replNames[] = {"Local", "Replicated", "Server Only"};
    Rectangle replRect = {panel.x + 8, y, panel.width - 16, 18};
    PropertyField::Style replStyle;
    DrawRectangleRec(replRect, replStyle.box);
    DrawRectangleLinesEx(replRect, 1, replStyle.border);
    DrawText(replNames[(int)var.replication], (int)replRect.x + 4, (int)replRect.y + 3, 11, replStyle.text);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), replRect) && !savePicker.IsOpen()) {
        var.replication = (ReplicationMode)(((int)var.replication + 1) % 3);
        isDirty = true;
    }
    y += 26;

    DrawText("Save File", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
    y += 14;
    Rectangle saveRect = {panel.x + 8, y, panel.width - 16, 18};
    PropertyField::Style saveStyle;
    DrawRectangleRec(saveRect, saveStyle.box);
    DrawRectangleLinesEx(saveRect, 1, saveStyle.border);
    std::string saveLabel = var.saveFile.empty() ? "None" : var.saveFile;
    DrawText(saveLabel.c_str(), (int)saveRect.x + 4, (int)saveRect.y + 3, 11, saveStyle.text);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), saveRect) && !savePicker.IsOpen()) {
        saveTargetFieldKey.clear();
        savePicker.Open({saveRect.x, saveRect.y + saveRect.height}, SaveFileRegistry::Category());
    }
    std::string pickedSaveFile;
    if (savePicker.Render(pickedSaveFile)) {
        var.saveFile = pickedSaveFile;
        isDirty = true;
    }
}

void StargraphEditorPanel::RenderRightPanel(Rectangle panel) {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(panel);

    float y = panel.y + 8;

    if (showSelfActorDetails) {
        DrawText(graph.assetName.c_str(), (int)panel.x + 8, (int)y, 13, theme.textPrimary);
        y += 20;
        DrawText("Actor properties", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
        return;
    }

    std::vector<ComponentDecl>& comps = graph.GetComponents();
    if (lastValidComponentIndex >= (int)comps.size()) lastValidComponentIndex = -1;
    if (lastValidComponentIndex >= 0) {
        RenderComponentDetails(panel, y);
        return;
    }

    std::vector<Variable>& vars = graph.GetVariables();
    if (lastValidVariableIndex >= (int)vars.size()) lastValidVariableIndex = -1;
    if (lastValidVariableIndex >= 0) {
        RenderVariableDetailsBody(panel, y, vars[lastValidVariableIndex], lastValidVariableIndex, false);
        return;
    }

    std::vector<FunctionDef>& funcsForDetails = graph.GetFunctions();
    if (lastValidFunctionIndex >= (int)funcsForDetails.size()) lastValidFunctionIndex = -1;
    if (lastValidFunctionIndex >= 0) {
        RenderFunctionDetails(panel, y, funcsForDetails[lastValidFunctionIndex], lastValidFunctionIndex);
        return;
    }

    if (openFunctionIndex >= 0 && openFunctionIndex < (int)graph.GetFunctions().size()) {
        std::vector<Variable>& localVars = ActiveGraph().GetVariables();
        if (lastValidLocalVariableIndex >= (int)localVars.size()) lastValidLocalVariableIndex = -1;
        if (lastValidLocalVariableIndex >= 0) {
            RenderVariableDetailsBody(panel, y, localVars[lastValidLocalVariableIndex], lastValidLocalVariableIndex, true);
            return;
        }
    }

    if (selectedNodeId == -1) {
        DrawText("Nothing selected", (int)panel.x + 8, (int)y, 11, theme.textSecondary);
        return;
    }

    Node* selectedNode = ActiveGraph().FindNodeById(selectedNodeId);
    if (selectedNode && selectedNode->type == "Custom_Event") {
        RenderCustomEventDetails(panel, y, *selectedNode);
        return;
    }
    if (selectedNode && selectedNode->type == "Call_Custom_Event") {
        RenderCallCustomEventDetails(panel, y, *selectedNode);
        return;
    }
    if (selectedNode && selectedNode->type == "Call_Function") {
        RenderCallFunctionDetails(panel, y, *selectedNode);
        return;
    }

    for (const Node& node : ActiveGraph().GetNodes()) {
        if (std::stoi(node.id) != selectedNodeId) continue;

        DrawText(node.title.c_str(), (int)panel.x + 8, (int)y, 13, theme.textPrimary);
        y += 20;
        DrawText(node.type.c_str(), (int)panel.x + 8, (int)y, 11, theme.textSecondary);
        y += 18;
        DrawLine((int)panel.x + 4, (int)y, (int)(panel.x + panel.width - 4), (int)y, theme.panelBorder);
        y += 8;
        for (const Pin& pin : node.inputs) {
            if (pin.type == PinType::Exec) continue;
            if (pin.name == "Variable_Name") continue;
            if (pin.name == "Component_Name") continue;
            DrawText(pin.name.c_str(), (int)panel.x + 8, (int)y, 11, theme.textSecondary);
            y += 14;
            if ((node.type == "Play_Sound" || node.type == "Load_Music") && pin.name == "Path") {
                Rectangle r = {panel.x + 8, y, panel.width - 16, 18};
                DrawRectangleRec(r, theme.fieldBg);
                DrawRectangleLinesEx(r, 1, theme.fieldBorder);
                std::string pathLabel = pin.defaultValue.empty() ? "None" : std::filesystem::path((std::string)pin.defaultValue).stem().string();
                DrawText(pathLabel.c_str(), (int)r.x + 4, (int)r.y + 3, 11, theme.textPrimary);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), r)) {
                    audioPathPicker.Open({r.x, r.y + r.height}, ".starsound");
                }
                std::string pickedPath;
                if (audioPathPicker.Render(pickedPath)) {
                    Pin* editablePin = ActiveGraph().FindPin(pin.id);
                    if (editablePin) {
                        editablePin->defaultValue = pickedPath;
                        isDirty = true;
                    }
                }
                y += 22;
            } else {
                DrawText(pin.defaultValue.empty() ? "(empty)" : pin.defaultValue.c_str(),
                         (int)panel.x + 12, (int)y, 11, theme.textPrimary);
                y += 16;
            }
        }
        break;
    }
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