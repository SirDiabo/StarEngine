#include "StargraphEditorPanel.h"
#include "Editor/PropertyRowBuilders.h"
#include "Runtime/ComponentRegistry.h"
#include "Editor/ComponentEditing.h"
#include "Modules/Stargraph/NodeRegistry.h"
#include "Core/InputKeyNames.h"
#include "Core/InputMappingRegistry.h"
#include "Core/StarAsset.h"
#include "Core/SaveFileRegistry.h"
#include "Core/SafeParse.h"
#include "Core/CollisionLayers.h"
#include "Editor/UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/TreeListElement.h"
#include "Core/Widgets/VerticalBoxElement.h"
#include "Core/Widgets/TextElement.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <filesystem>

using namespace PropertyRows;

namespace {

const char* ParamTypeShortName(PinType type) {
    switch (type) {
        case PinType::Boolean: return "Bool";
        case PinType::Integer: return "Int";
        case PinType::Float:   return "Float";
        case PinType::String:  return "String";
        case PinType::Vec3:    return "Vec3";
        case PinType::Rotator: return "Rotator";
        case PinType::Object:  return "Object";
        default:                return "Float";
    }
}

PinType NextParamType(PinType type) {
    switch (type) {
        case PinType::Boolean: return PinType::Integer;
        case PinType::Integer: return PinType::Float;
        case PinType::Float:   return PinType::String;
        case PinType::String:  return PinType::Vec3;
        case PinType::Vec3:    return PinType::Rotator;
        case PinType::Rotator: return PinType::Object;
        default:                return PinType::Boolean;
    }
}

std::string FormatVec3Text(float x, float y, float z) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f", x, y, z);
    return buf;
}

void ParseVec3Text(const std::string& text, float& x, float& y, float& z) {
    x = y = z = 0.0f;
    sscanf(text.c_str(), "%f,%f,%f", &x, &y, &z);
}

Pin* NthParamPin(std::vector<Pin>& pins, int ordinal) {
    int seen = 0;
    for (Pin& pin : pins) {
        if (pin.type == PinType::Exec) continue;
        if (seen == ordinal) return &pin;
        seen++;
    }
    return nullptr;
}

std::string UniquePinName(const std::vector<Pin>& pins, const std::string& base) {
    std::string name;
    int n = 1;
    do {
        name = base + std::to_string(n++);
        bool taken = false;
        for (const Pin& p : pins) if (p.name == name) { taken = true; break; }
        if (!taken) break;
    } while (n < 1000);
    return name;
}

const std::vector<std::string>& ReplicationOptions() {
    static const std::vector<std::string> options = { "Local", "Replicated", "Server Only" };
    return options;
}

int ReplicationIndex(const std::string& name) {
    const std::vector<std::string>& options = ReplicationOptions();
    for (size_t i = 0; i < options.size(); i++) if (options[i] == name) return (int)i;
    return 0;
}

}

Variable* StargraphEditorPanel::DetailVariable(int index, bool isLocal) {
    std::vector<Variable>& vars = isLocal ? ActiveGraph().GetVariables() : graph.GetVariables();
    return (index >= 0 && index < (int)vars.size()) ? &vars[index] : nullptr;
}

FunctionDef* StargraphEditorPanel::DetailFunction(int index) {
    std::vector<FunctionDef>& funcs = graph.GetFunctions();
    return (index >= 0 && index < (int)funcs.size()) ? &funcs[index] : nullptr;
}

Node* StargraphEditorPanel::DetailNode(int nodeId) {
    return ActiveGraph().FindNodeById(nodeId);
}

void StargraphEditorPanel::ResetDetailPickers() {
    savePicker.Close();
    layerPicker.Close();
    saveTargetIndex = -1;
    actorEditor.ResetPickers();
}

void StargraphEditorPanel::RenderDetailPickers(Rectangle panel) {
    actorEditor.RenderPickers(panel);

    std::string pickedLayer;
    if (layerPicker.Render(pickedLayer)) {
        if (Pin* layerPin = ActiveGraph().FindPin(layerPickerPinId)) {
            layerPin->defaultValue = pickedLayer.empty() ? std::string("All") : pickedLayer;
            isDirty = true;
        }
    }

    std::string pickedSaveFile;
    if (savePicker.Render(pickedSaveFile)) {
        if (Variable* v = DetailVariable(saveTargetIndex, false)) {
            v->saveFile = pickedSaveFile;
            isDirty = true;
        }
    }
}

namespace {

using PinListGetter = std::function<std::vector<Pin>*()>;

void AppendParamRows(std::vector<PropertyRow>& rows, const std::string& idPrefix, const std::vector<Pin>& pins,
                     PinListGetter getter, std::function<void(const std::string&)> removePin,
                     std::function<void()> markDirty, Color dangerColor) {
    int ordinal = 0;
    for (const Pin& pin : pins) {
        if (pin.type == PinType::Exec) continue;
        int k = ordinal++;
        rows.push_back(CellsRow(idPrefix + std::to_string(k), {
            TextCell(pin.name, 0.0f, "", [getter, markDirty, k](const std::string& text) {
                std::vector<Pin>* list = getter();
                if (!list) return;
                Pin* p = NthParamPin(*list, k);
                if (!p) return;
                p->name = text;
                markDirty();
            }),
            ButtonCell(ParamTypeShortName(pin.type), 40.0f, [getter, markDirty, k](Rectangle) {
                std::vector<Pin>* list = getter();
                if (!list) return;
                Pin* p = NthParamPin(*list, k);
                if (!p) return;
                p->type = NextParamType(p->type);
                markDirty();
            }),
            ButtonCell("X", 22.0f, [getter, removePin, k](Rectangle) {
                std::vector<Pin>* list = getter();
                if (!list) return;
                Pin* p = NthParamPin(*list, k);
                if (!p) return;
                std::string name = p->name;
                removePin(name);
            }, dangerColor)
        }));
    }
}

}

void StargraphEditorPanel::BuildVariableRows(std::vector<PropertyRow>& rows, int idx, bool isLocal) {
    Variable& var = isLocal ? ActiveGraph().GetVariables()[idx] : graph.GetVariables()[idx];
    const UIPalette& theme = UITheme::Get();
    const std::string prefix = isLocal ? "lv." : "v.";

    struct TypeEntry { const char* name; PinType type; };
    static const TypeEntry kTypes[] = {
        { "Boolean", PinType::Boolean }, { "Integer", PinType::Integer }, { "Float", PinType::Float },
        { "String", PinType::String }, { "Vec3", PinType::Vec3 }, { "Rotator", PinType::Rotator }, { "Object", PinType::Object }
    };

    rows.push_back(StringRow(prefix + "name", "Name", var.name, [this, idx, isLocal](const std::string& value) {
        if (Variable* v = DetailVariable(idx, isLocal)) {
            v->name = value;
            isDirty = true;
        }
    }));

    std::vector<std::string> typeNames;
    std::string currentType = "Boolean";
    for (const TypeEntry& t : kTypes) {
        typeNames.push_back(t.name);
        if (t.type == var.type) currentType = t.name;
    }
    rows.push_back(EnumRow(prefix + "type", "Type", currentType, typeNames, [this, idx, isLocal](const std::string& value) {
        Variable* v = DetailVariable(idx, isLocal);
        if (!v) return;
        for (const TypeEntry& t : kTypes) {
            if (value == t.name) { v->type = t.type; break; }
        }
        switch (v->type) {
            case PinType::Boolean: v->value = "false"; break;
            case PinType::Integer: v->value = "0"; break;
            case PinType::Float:   v->value = "0.00"; break;
            case PinType::Vec3:    v->value = "0.00,0.00,0.00"; break;
            case PinType::Rotator: v->value = "0.00,0.00,0.00"; break;
            default:               v->value = ""; break;
        }
        v->arrayValues.clear();
        isDirty = true;
    }));

    if (var.type == PinType::Object) {
        std::string kindLabel = var.objectKind.empty() ? std::string("Any") : std::filesystem::path(var.objectKind).stem().string();
        PropertyRow kindRow = AssetRow(prefix + "kind", "Kind", ".staractor", kindLabel);
        if (!var.objectKind.empty()) {
            kindRow.trailingLabel = "x";
            kindRow.trailingColor = theme.dangerBg;
            kindRow.onTrailingPressed = [this, idx, isLocal](Rectangle) {
                if (Variable* v = DetailVariable(idx, isLocal)) {
                    v->objectKind.clear();
                    isDirty = true;
                }
            };
        }
        rows.push_back(std::move(kindRow));
    }

    rows.push_back(BoolRow(prefix + "isarray", "Array", var.isArray, [this, idx, isLocal](bool value) {
        Variable* v = DetailVariable(idx, isLocal);
        if (!v || v->isArray == value) return;
        v->isArray = value;
        if (v->isArray) {
            v->arrayValues.clear();
            v->arrayValues.push_back(v->value.empty() ? v->DefaultElementText() : v->value);
        } else {
            v->value = v->arrayValues.empty() ? v->DefaultElementText() : v->arrayValues.front();
            v->arrayValues.clear();
        }
        isDirty = true;
    }));

    rows.push_back(HeaderRow(prefix + "default", "Default Value"));

    if (var.isArray) {
        rows.push_back(CellsRow(prefix + "arr.h", {
            LabelCell("Elements (" + std::to_string(var.arrayValues.size()) + ")", 0.0f),
            ButtonCell("+ Add", 60.0f, [this, idx, isLocal](Rectangle) {
                if (Variable* v = DetailVariable(idx, isLocal)) {
                    v->arrayValues.push_back(v->DefaultElementText());
                    isDirty = true;
                }
            })
        }));

        auto setElement = [this, idx, isLocal](int index, const std::string& text) {
            Variable* v = DetailVariable(idx, isLocal);
            if (!v || index < 0 || index >= (int)v->arrayValues.size()) return;
            v->arrayValues[index] = text;
            isDirty = true;
        };

        for (size_t i = 0; i < var.arrayValues.size(); i++) {
            int e = (int)i;
            const std::string& elem = var.arrayValues[i];
            std::vector<PropertyCell> cells;
            cells.push_back(LabelCell(std::to_string(i), 22.0f));

            switch (var.type) {
                case PinType::Boolean: {
                    bool value = (elem == "true" || elem == "1");
                    cells.push_back(ButtonCell(value ? "true" : "false", 0.0f, [this, idx, isLocal, e](Rectangle) {
                        Variable* v = DetailVariable(idx, isLocal);
                        if (!v || e < 0 || e >= (int)v->arrayValues.size()) return;
                        bool current = (v->arrayValues[e] == "true" || v->arrayValues[e] == "1");
                        v->arrayValues[e] = current ? "false" : "true";
                        isDirty = true;
                    }));
                    break;
                }
                case PinType::Integer:
                    cells.push_back(NumberCell((float)atoi(elem.c_str()), true, 0.0f, [setElement, e](float value) {
                        setElement(e, std::to_string((int)std::lround(value)));
                    }));
                    break;
                case PinType::Float:
                    cells.push_back(NumberCell((float)atof(elem.c_str()), false, 0.0f, [setElement, e](float value) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%.2f", value);
                        setElement(e, buf);
                    }));
                    break;
                case PinType::Vec3:
                case PinType::Rotator: {
                    float vec[3];
                    ParseVec3Text(elem, vec[0], vec[1], vec[2]);
                    for (int axis = 0; axis < 3; axis++) {
                        cells.push_back(NumberCell(vec[axis], false, 0.0f, [this, idx, isLocal, e, axis, setElement](float value) {
                            Variable* v = DetailVariable(idx, isLocal);
                            if (!v || e < 0 || e >= (int)v->arrayValues.size()) return;
                            float c[3];
                            ParseVec3Text(v->arrayValues[e], c[0], c[1], c[2]);
                            c[axis] = value;
                            setElement(e, FormatVec3Text(c[0], c[1], c[2]));
                        }));
                    }
                    break;
                }
                default:
                    cells.push_back(TextCell(elem, 0.0f, "(empty)", [setElement, e](const std::string& text) { setElement(e, text); }));
                    break;
            }

            cells.push_back(ButtonCell("^", 18.0f, [this, idx, isLocal, e](Rectangle) {
                Variable* v = DetailVariable(idx, isLocal);
                if (!v || e <= 0 || e >= (int)v->arrayValues.size()) return;
                std::swap(v->arrayValues[e], v->arrayValues[e - 1]);
                isDirty = true;
            }));
            cells.push_back(ButtonCell("v", 18.0f, [this, idx, isLocal, e](Rectangle) {
                Variable* v = DetailVariable(idx, isLocal);
                if (!v || e < 0 || e + 1 >= (int)v->arrayValues.size()) return;
                std::swap(v->arrayValues[e], v->arrayValues[e + 1]);
                isDirty = true;
            }));
            cells.push_back(ButtonCell("x", 18.0f, [this, idx, isLocal, e](Rectangle) {
                Variable* v = DetailVariable(idx, isLocal);
                if (!v || e < 0 || e >= (int)v->arrayValues.size()) return;
                v->arrayValues.erase(v->arrayValues.begin() + e);
                isDirty = true;
            }, theme.dangerBg));

            rows.push_back(CellsRow(prefix + "arr." + std::to_string(i), std::move(cells)));
        }
    } else if (var.type == PinType::Boolean) {
        rows.push_back(BoolRow(prefix + "value", "Value", var.value == "true" || var.value == "1", [this, idx, isLocal](bool value) {
            if (Variable* v = DetailVariable(idx, isLocal)) { v->value = value ? "true" : "false"; isDirty = true; }
        }));
    } else if (var.type == PinType::Integer) {
        rows.push_back(FloatRow(prefix + "value", "Value", (float)atoi(var.value.c_str()), true, [this, idx, isLocal](float value) {
            if (Variable* v = DetailVariable(idx, isLocal)) { v->value = std::to_string((int)std::lround(value)); isDirty = true; }
        }));
    } else if (var.type == PinType::Float) {
        rows.push_back(FloatRow(prefix + "value", "Value", (float)atof(var.value.c_str()), false, [this, idx, isLocal](float value) {
            if (Variable* v = DetailVariable(idx, isLocal)) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.2f", value);
                v->value = buf;
                isDirty = true;
            }
        }));
    } else if (var.type == PinType::Vec3 || var.type == PinType::Rotator) {
        float x, y, z;
        ParseVec3Text(var.value, x, y, z);
        rows.push_back(Vec3Row(prefix + "value", "Value", x, y, z, [this, idx, isLocal](float vx, float vy, float vz) {
            if (Variable* v = DetailVariable(idx, isLocal)) { v->value = FormatVec3Text(vx, vy, vz); isDirty = true; }
        }));
    } else {
        rows.push_back(StringRow(prefix + "value", "Value", var.value, [this, idx, isLocal](const std::string& text) {
            if (Variable* v = DetailVariable(idx, isLocal)) { v->value = text; isDirty = true; }
        }, "(empty)"));
    }

    if (isLocal) return;

    rows.push_back(EnumRow(prefix + "repl", "Replication", ReplicationOptions()[(int)var.replication], ReplicationOptions(),
        [this, idx](const std::string& name) {
            if (Variable* v = DetailVariable(idx, false)) {
                v->replication = (ReplicationMode)ReplicationIndex(name);
                isDirty = true;
            }
        }));

    rows.push_back(CellsRow(prefix + "save", {
        LabelCell("Save File", 90.0f),
        ButtonCell(var.saveFile.empty() ? std::string("None") : var.saveFile, 0.0f, [this, idx](Rectangle r) {
            saveTargetIndex = idx;
            savePicker.Open({ r.x, r.y + r.height }, SaveFileRegistry::Category());
        })
    }));
}

void StargraphEditorPanel::BuildFunctionRows(std::vector<PropertyRow>& rows, int fnIdx) {
    FunctionDef& fn = graph.GetFunctions()[fnIdx];
    const UIPalette& theme = UITheme::Get();
    auto markDirty = [this]() { isDirty = true; };

    rows.push_back(StringRow("fn.name", "Name", fn.name, [this, fnIdx](const std::string& value) {
        if (!value.empty()) graph.RenameFunction((size_t)fnIdx, value);
        isDirty = true;
    }));

    Node* entryNode = fn.graph.FindNodeByType("Function_Entry");
    Node* returnNode = fn.graph.FindNodeByType("Return");

    rows.push_back(HeaderRow("fn.in.h", "Inputs"));
    if (entryNode) {
        AppendParamRows(rows, "fn.in.", entryNode->outputs,
            [this, fnIdx]() -> std::vector<Pin>* {
                FunctionDef* f = DetailFunction(fnIdx);
                Node* e = f ? f->graph.FindNodeByType("Function_Entry") : nullptr;
                return e ? &e->outputs : nullptr;
            },
            [this, fnIdx](const std::string& name) {
                FunctionDef* f = DetailFunction(fnIdx);
                Node* e = f ? f->graph.FindNodeByType("Function_Entry") : nullptr;
                if (!e) return;
                f->graph.RemoveDynamicPin(*e, true, name);
                isDirty = true;
            },
            markDirty, theme.dangerBg);
        rows.push_back(ButtonRow("fn.in.add", "+ Add Input", [this, fnIdx]() {
            FunctionDef* f = DetailFunction(fnIdx);
            Node* e = f ? f->graph.FindNodeByType("Function_Entry") : nullptr;
            if (!e) return;
            f->graph.AddDynamicPin(*e, true, UniquePinName(e->outputs, "Input"), PinType::Float);
            isDirty = true;
        }));
    }

    rows.push_back(HeaderRow("fn.out.h", "Outputs"));
    if (returnNode) {
        AppendParamRows(rows, "fn.out.", returnNode->inputs,
            [this, fnIdx]() -> std::vector<Pin>* {
                FunctionDef* f = DetailFunction(fnIdx);
                Node* r = f ? f->graph.FindNodeByType("Return") : nullptr;
                return r ? &r->inputs : nullptr;
            },
            [this, fnIdx](const std::string& name) {
                FunctionDef* f = DetailFunction(fnIdx);
                Node* r = f ? f->graph.FindNodeByType("Return") : nullptr;
                if (!r) return;
                f->graph.RemoveDynamicPin(*r, false, name);
                isDirty = true;
            },
            markDirty, theme.dangerBg);
        rows.push_back(ButtonRow("fn.out.add", "+ Add Output", [this, fnIdx]() {
            FunctionDef* f = DetailFunction(fnIdx);
            Node* r = f ? f->graph.FindNodeByType("Return") : nullptr;
            if (!r) return;
            f->graph.AddDynamicPin(*r, false, UniquePinName(r->inputs, "Output"), PinType::Float);
            isDirty = true;
        }));
    }
}

void StargraphEditorPanel::BuildCustomEventRows(std::vector<PropertyRow>& rows, int nodeId) {
    Node* node = DetailNode(nodeId);
    if (!node) return;
    const UIPalette& theme = UITheme::Get();

    rows.push_back(StringRow("ev.name", "Event Name", node->title, [this, nodeId](const std::string& value) {
        if (Node* n = DetailNode(nodeId)) { n->title = value; isDirty = true; }
    }));
    rows.push_back(EnumRow("ev.repl", "Replication", ReplicationOptions()[(int)node->eventReplication], ReplicationOptions(),
        [this, nodeId](const std::string& name) {
            if (Node* n = DetailNode(nodeId)) { n->eventReplication = (ReplicationMode)ReplicationIndex(name); isDirty = true; }
        }));
    if (node->eventReplication != ReplicationMode::Local) {
        rows.push_back(BoolRow("ev.rel", "Reliable", node->eventReliable, [this, nodeId](bool value) {
            if (Node* n = DetailNode(nodeId)) { n->eventReliable = value; isDirty = true; }
        }));
    }

    rows.push_back(HeaderRow("ev.h", "Parameters"));
    AppendParamRows(rows, "ev.p.", node->outputs,
        [this, nodeId]() -> std::vector<Pin>* {
            Node* n = DetailNode(nodeId);
            return n ? &n->outputs : nullptr;
        },
        [this, nodeId](const std::string& name) {
            Node* n = DetailNode(nodeId);
            if (!n) return;
            graph.RemoveDynamicPin(*n, true, name);
            isDirty = true;
        },
        [this]() { isDirty = true; }, theme.dangerBg);
    rows.push_back(ButtonRow("ev.add", "+ Add Parameter", [this, nodeId]() {
        Node* n = DetailNode(nodeId);
        if (!n) return;
        graph.AddDynamicPin(*n, true, UniquePinName(n->outputs, "Param"), PinType::Float);
        isDirty = true;
    }));
}

void StargraphEditorPanel::BuildCallCustomEventRows(std::vector<PropertyRow>& rows, int nodeId) {
    Node* node = DetailNode(nodeId);
    if (!node) return;

    std::string eventName;
    for (const Pin& p : node->inputs) if (p.name == "Event Name") eventName = p.defaultValue.c_str();

    rows.push_back(StringRow("cce.name", "Event Name", eventName, [this, nodeId](const std::string& value) {
        Node* n = DetailNode(nodeId);
        if (!n) return;
        for (Pin& p : n->inputs) if (p.name == "Event Name") { p.defaultValue = value; isDirty = true; }
    }));

    rows.push_back(ButtonRow("cce.refresh", "Refresh Pins", [this, nodeId]() {
        Node* n = DetailNode(nodeId);
        if (!n) return;
        std::string targetName;
        for (const Pin& p : n->inputs) if (p.name == "Event Name") targetName = p.defaultValue.c_str();
        Node* targetEvent = graph.FindCustomEventByName(targetName);
        if (!targetEvent) return;
        std::vector<std::string> keep = { "Target", "Event Name" };
        for (const Pin& srcPin : targetEvent->outputs) {
            if (srcPin.type == PinType::Exec) continue;
            ActiveGraph().AddDynamicPin(*n, false, srcPin.name, srcPin.type);
            keep.push_back(srcPin.name);
        }
        std::vector<std::string> toRemove;
        for (const Pin& p : n->inputs) {
            if (p.type == PinType::Exec) continue;
            if (std::find(keep.begin(), keep.end(), p.name) == keep.end()) toRemove.push_back(p.name);
        }
        for (const std::string& name : toRemove) ActiveGraph().RemoveDynamicPin(*n, false, name);
        isDirty = true;
    }));

    rows.push_back(HeaderRow("cce.h", "Parameters"));
    int k = 0;
    for (const Pin& pin : node->inputs) {
        if (pin.type == PinType::Exec) continue;
        if (pin.name == "Target" || pin.name == "Event Name") continue;
        rows.push_back(ReadOnlyRow("cce.p" + std::to_string(k++), pin.name, ParamTypeShortName(pin.type)));
    }
}

void StargraphEditorPanel::BuildCallFunctionRows(std::vector<PropertyRow>& rows, int nodeId) {
    Node* node = DetailNode(nodeId);
    if (!node) return;

    std::string functionName;
    for (const Pin& p : node->inputs) if (p.name == "Function Name") functionName = p.defaultValue.c_str();

    rows.push_back(StringRow("cf.name", "Function Name", functionName, [this, nodeId](const std::string& value) {
        Node* n = DetailNode(nodeId);
        if (!n) return;
        for (Pin& p : n->inputs) if (p.name == "Function Name") { p.defaultValue = value; isDirty = true; }
    }));

    rows.push_back(ButtonRow("cf.refresh", "Refresh Pins", [this, nodeId]() {
        Node* n = DetailNode(nodeId);
        if (!n) return;
        std::string targetName;
        for (const Pin& p : n->inputs) if (p.name == "Function Name") targetName = p.defaultValue.c_str();
        FunctionDef* targetFn = graph.FindFunction(targetName);
        if (!targetFn) return;
        Node* entryNode = targetFn->graph.FindNodeByType("Function_Entry");
        Node* returnNode = targetFn->graph.FindNodeByType("Return");

        std::vector<std::string> keepInputs = { "Function Name" };
        if (entryNode) {
            for (const Pin& srcPin : entryNode->outputs) {
                if (srcPin.type == PinType::Exec) continue;
                ActiveGraph().AddDynamicPin(*n, false, srcPin.name, srcPin.type);
                keepInputs.push_back(srcPin.name);
            }
        }
        std::vector<std::string> toRemoveIn;
        for (const Pin& p : n->inputs) {
            if (p.type == PinType::Exec) continue;
            if (std::find(keepInputs.begin(), keepInputs.end(), p.name) == keepInputs.end()) toRemoveIn.push_back(p.name);
        }
        for (const std::string& name : toRemoveIn) ActiveGraph().RemoveDynamicPin(*n, false, name);

        std::vector<std::string> keepOutputs;
        if (returnNode) {
            for (const Pin& srcPin : returnNode->inputs) {
                if (srcPin.type == PinType::Exec) continue;
                ActiveGraph().AddDynamicPin(*n, true, srcPin.name, srcPin.type);
                keepOutputs.push_back(srcPin.name);
            }
        }
        std::vector<std::string> toRemoveOut;
        for (const Pin& p : n->outputs) {
            if (p.type == PinType::Exec) continue;
            if (std::find(keepOutputs.begin(), keepOutputs.end(), p.name) == keepOutputs.end()) toRemoveOut.push_back(p.name);
        }
        for (const std::string& name : toRemoveOut) ActiveGraph().RemoveDynamicPin(*n, true, name);
        isDirty = true;
    }));

    rows.push_back(HeaderRow("cf.in.h", "Inputs"));
    int k = 0;
    for (const Pin& pin : node->inputs) {
        if (pin.type == PinType::Exec || pin.name == "Function Name") continue;
        rows.push_back(ReadOnlyRow("cf.in." + std::to_string(k++), pin.name, ParamTypeShortName(pin.type)));
    }
    rows.push_back(HeaderRow("cf.out.h", "Outputs"));
    k = 0;
    for (const Pin& pin : node->outputs) {
        if (pin.type == PinType::Exec) continue;
        rows.push_back(ReadOnlyRow("cf.out." + std::to_string(k++), pin.name, ParamTypeShortName(pin.type)));
    }
}

void StargraphEditorPanel::BuildGenericNodeRows(std::vector<PropertyRow>& rows, int nodeId) {
    Node* node = DetailNode(nodeId);
    if (!node) return;
    rows.push_back(HeaderRow("n.title", node->title));
    rows.push_back(ReadOnlyRow("n.type", "Type", node->type));

    const NodeTemplate* nodeTemplate = NodeRegistry::Get().GetTemplate(node->type);
    if (nodeTemplate && nodeTemplate->dynamicExecOutputs) {
        const UIPalette& theme = UITheme::Get();
        rows.push_back(HeaderRow("n.cases.h", nodeTemplate->dynamicExecHeader.empty() ? std::string("Outputs") : nodeTemplate->dynamicExecHeader));
        if (!nodeTemplate->dynamicExecFixedPin.empty()) {
            rows.push_back(ReadOnlyRow("n.cases.fixed", nodeTemplate->dynamicExecFixedPin, "Always present"));
        }
        for (size_t i = 0; i < node->execCases.size(); i++) {
            int caseIndex = (int)i;
            std::vector<PropertyCell> cells;
            if (nodeTemplate->normalizeExecCase) {
                cells.push_back(TextCell(node->execCases[i], 0.0f, nodeTemplate->dynamicExecPlaceholder, [this, nodeId, caseIndex](const std::string& text) {
                    Node* n = DetailNode(nodeId);
                    if (n && ActiveGraph().SetExecCase(*n, caseIndex, text)) isDirty = true;
                }));
            } else {
                cells.push_back(LabelCell("Output " + std::to_string(caseIndex), 0.0f));
            }
            cells.push_back(ButtonCell("X", 22.0f, [this, nodeId, caseIndex](Rectangle) {
                Node* n = DetailNode(nodeId);
                if (n && ActiveGraph().RemoveExecCase(*n, caseIndex)) isDirty = true;
            }, theme.dangerBg));
            rows.push_back(CellsRow("n.case." + std::to_string(i), std::move(cells)));
        }
        rows.push_back(ButtonRow("n.cases.add", nodeTemplate->dynamicExecAddLabel.empty() ? std::string("+ Add") : nodeTemplate->dynamicExecAddLabel, [this, nodeId]() {
            Node* n = DetailNode(nodeId);
            if (!n) return;
            const NodeTemplate* tmpl = NodeRegistry::Get().GetTemplate(n->type);
            if (!tmpl) return;
            std::string initial = tmpl->newExecCaseDefault ? tmpl->newExecCaseDefault(n->execCases) : std::string();
            if (ActiveGraph().AddExecCase(*n, initial)) isDirty = true;
        }));
    }

    if (nodeTemplate && !nodeTemplate->dynamicWildcardInputPrefix.empty()) {
        const UIPalette& theme = UITheme::Get();
        const std::string prefix = nodeTemplate->dynamicWildcardInputPrefix;
        rows.push_back(HeaderRow("n.items.h", "Items"));
        int itemIndex = 0;
        for (const Pin& pin : node->inputs) {
            if (pin.name.rfind(prefix, 0) != 0) continue;
            int pinId = pin.id;
            std::vector<PropertyCell> cells;
            cells.push_back(LabelCell(pin.name, 0.0f));
            cells.push_back(ButtonCell("X", 22.0f, [this, nodeId, pinId](Rectangle) {
                Node* n = DetailNode(nodeId);
                if (n && ActiveGraph().RemoveDynamicWildcardInput(*n, pinId)) {
                    RefreshWildcardPins(n);
                    isDirty = true;
                }
            }, theme.dangerBg));
            rows.push_back(CellsRow("n.item." + std::to_string(itemIndex++), std::move(cells)));
        }
        rows.push_back(ButtonRow("n.items.add", "+ Add Item", [this, nodeId]() {
            Node* n = DetailNode(nodeId);
            if (n && ActiveGraph().AddDynamicWildcardInput(*n)) {
                RefreshWildcardPins(n);
                isDirty = true;
            }
        }));
    }

    for (const Pin& pin : node->inputs) {
        if (pin.type == PinType::Exec || pin.type == PinType::Array) continue;
        if (pin.name == "Variable_Name" || pin.name == "Component_Name") continue;
        std::string value = pin.defaultValue.c_str();
        bool isInterpNode = node->type == "Float_Interp_To" || node->type == "Int_Interp_To" ||
                             node->type == "Vec3_Interp_To" || node->type == "Rotator_Interp_To";
        std::string assetExtension = StargraphPinAssetExtension(pin);
        if (!assetExtension.empty()) {
            rows.push_back(AssetRow("pin:" + std::to_string(pin.id), pin.name, assetExtension, StemOrNone(value)));
        } else if (node->type == "Line_Trace_For_Mesh" && pin.name == "Channel") {
            int pinId = pin.id;
            rows.push_back(CellsRow("n.p" + std::to_string(pin.id), {
                LabelCell(pin.name, 90.0f),
                ButtonCell(value.empty() ? std::string("All") : value, 0.0f, [this, pinId](Rectangle r) {
                    layerPickerPinId = pinId;
                    layerPicker.OpenList({ r.x, r.y + r.height }, CollisionLayers::Names(), "All");
                })
            }));
        } else if (isInterpNode && pin.name == "Method") {
            int pinId = pin.id;
            rows.push_back(EnumRow("n.p" + std::to_string(pin.id), pin.name, value.empty() ? "Exponential" : value,
                {"Exponential", "Linear"}, [this, pinId](const std::string& v) {
                    if (Pin* p = ActiveGraph().FindPin(pinId)) { p->defaultValue = v; isDirty = true; }
                }));
        } else {
            rows.push_back(ReadOnlyRow("n.p" + std::to_string(pin.id), pin.name, value.empty() ? std::string("(empty)") : value));
        }
    }
}

std::string StargraphEditorPanel::DetailsSelectionKey() {
    if (showSelfActorDetails) return "self";

    if (actorEditor.HasDetailsSelection()) return actorEditor.DetailsSelectionKey();
    if (lastValidVariableIndex >= 0 && lastValidVariableIndex < (int)graph.GetVariables().size())
        return "v" + std::to_string(lastValidVariableIndex);
    if (lastValidFunctionIndex >= 0 && lastValidFunctionIndex < (int)graph.GetFunctions().size())
        return "f" + std::to_string(lastValidFunctionIndex);

    if (openFunctionIndex >= 0 && openFunctionIndex < (int)graph.GetFunctions().size()) {
        if (lastValidLocalVariableIndex >= 0 && lastValidLocalVariableIndex < (int)ActiveGraph().GetVariables().size())
            return "l" + std::to_string(lastValidLocalVariableIndex);
    }

    if (selectedNodeId != -1) {
        Node* selectedNode = ActiveGraph().FindNodeById(selectedNodeId);
        if (selectedNode) return "n" + std::to_string(selectedNodeId) + ":" + selectedNode->type;
    }
    return "";
}

std::vector<PropertyRow> StargraphEditorPanel::BuildDetailsRows() {
    std::vector<PropertyRow> rows;

    actorEditor.ClampSelection();
    std::vector<Variable>& vars = graph.GetVariables();
    if (lastValidVariableIndex >= (int)vars.size()) lastValidVariableIndex = -1;
    std::vector<FunctionDef>& funcs = graph.GetFunctions();
    if (lastValidFunctionIndex >= (int)funcs.size()) lastValidFunctionIndex = -1;

    bool localVariableSelected = false;
    if (openFunctionIndex >= 0 && openFunctionIndex < (int)funcs.size()) {
        std::vector<Variable>& localVars = ActiveGraph().GetVariables();
        if (lastValidLocalVariableIndex >= (int)localVars.size()) lastValidLocalVariableIndex = -1;
        localVariableSelected = lastValidLocalVariableIndex >= 0;
    }

    Node* selectedNode = (selectedNodeId != -1) ? ActiveGraph().FindNodeById(selectedNodeId) : nullptr;

    std::string key = DetailsSelectionKey();

    if (key != lastDetailsSelectionKey) {
        ResetDetailPickers();
        lastDetailsSelectionKey = key;
    }

    if (showSelfActorDetails) {
        rows.push_back(HeaderRow("self.h", graph.assetName));
        rows.push_back(ReadOnlyRow("self.d", "Type", "Actor properties"));
    } else if (actorEditor.HasDetailsSelection()) {
        actorEditor.BuildDetailsRows(rows);
    } else if (lastValidVariableIndex >= 0) {
        BuildVariableRows(rows, lastValidVariableIndex, false);
    } else if (lastValidFunctionIndex >= 0) {
        BuildFunctionRows(rows, lastValidFunctionIndex);
    } else if (localVariableSelected) {
        BuildVariableRows(rows, lastValidLocalVariableIndex, true);
    } else if (selectedNode) {
        if (selectedNode->type == "Custom_Event") BuildCustomEventRows(rows, selectedNodeId);
        else if (selectedNode->type == "Call_Custom_Event") BuildCallCustomEventRows(rows, selectedNodeId);
        else if (selectedNode->type == "Call_Function") BuildCallFunctionRows(rows, selectedNodeId);
        else BuildGenericNodeRows(rows, selectedNodeId);
    }

    for (PropertyRow& row : rows) row.id = key + "|" + row.id;
    return rows;
}

void StargraphEditorPanel::OnDetailAssetPicked(const std::string& fullRowId, const std::string& pickedPath) {
    size_t separator = fullRowId.find('|');
    const std::string rowId = (separator == std::string::npos) ? fullRowId : fullRowId.substr(separator + 1);
    if (actorEditor.OnAssetPicked(rowId, pickedPath)) return;
    if (rowId.rfind("pin:", 0) == 0) {
        Pin* pin = ActiveGraph().FindPin(atoi(rowId.c_str() + 4));
        if (pin) {
            pin->defaultValue = pickedPath;
            isDirty = true;
        }
    } else if (rowId.size() > 5 && rowId.compare(rowId.size() - 5, 5, ".kind") == 0) {
        bool local = rowId.rfind("lv.", 0) == 0;
        Variable* v = local ? DetailVariable(lastValidLocalVariableIndex, true) : DetailVariable(lastValidVariableIndex, false);
        if (v) {
            v->objectKind = pickedPath;
            isDirty = true;
        }
    }
}
