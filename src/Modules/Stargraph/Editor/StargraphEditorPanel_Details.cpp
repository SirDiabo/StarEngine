#include "StargraphEditorPanel.h"
#include "Modules/Stargraph/ComponentRegistry.h"
#include "Core/InputKeyNames.h"
#include "Core/InputMappingRegistry.h"
#include "Core/StarAsset.h"
#include "Core/SaveFileRegistry.h"
#include "Core/SafeParse.h"
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

void StargraphEditorPanel::BuildComponentChrome() {
    compChrome = new Widget();
    compRoot = new VerticalBoxElement();
    compRoot->childSpacing = 4.0f;
    compChrome->SetRoot(compRoot);

    compList = new TreeListElement();
    compList->dragKind = "StargraphComponent";
    compList->allowReorder = true;
    compList->allowReparent = true;
    compList->allowExternalDrag = true;
    compList->allowRename = false;
    compList->rowHeight = 18.0f;
    compList->scrollEnabled = true;

    compList->onEntrySelected = [this](const std::string& id) {
        std::vector<ComponentDecl>& comps = graph.GetComponents();
        for (size_t i = 0; i < comps.size(); i++) {
            if (comps[i].name == id) {
                selectedComponentIndex = (int)i;
                lastValidComponentIndex = (int)i;
                break;
            }
        }
        ClearOtherLeftSelections(3);
    };

    compList->onDeleteRequested = [this](const std::string& id) {
        std::vector<ComponentDecl>& comps = graph.GetComponents();
        for (size_t i = 0; i < comps.size(); i++) {
            if (comps[i].name != id) continue;
            for (ComponentDecl& child : comps) {
                if (child.parentName == id) child.parentName = comps[i].parentName;
            }
            graph.RemoveComponent(i);
            selectedComponentIndex = -1;
            lastValidComponentIndex = -1;
            isDirty = true;
            break;
        }
    };

    compList->onReorderOrReparent = [this](const std::string& draggedId, const std::string& targetId, TreeDropPosition pos) {
        std::vector<ComponentDecl>& comps = graph.GetComponents();
        int fromIndex = -1, toIndex = -1;
        for (size_t i = 0; i < comps.size(); i++) {
            if (comps[i].name == draggedId) fromIndex = (int)i;
            if (comps[i].name == targetId) toIndex = (int)i;
        }
        if (fromIndex < 0 || toIndex < 0) return;

        std::string checkName = targetId;
        while (!checkName.empty()) {
            if (checkName == draggedId) return;
            std::string nextParent;
            for (const ComponentDecl& c : comps) {
                if (c.name == checkName) { nextParent = c.parentName; break; }
            }
            checkName = nextParent;
        }

        if (pos == TreeDropPosition::Into) {
            comps[fromIndex].parentName = targetId;
        } else {
            comps[fromIndex].parentName = comps[toIndex].parentName;
            MoveInList(comps, fromIndex, toIndex, pos);
        }
        isDirty = true;
    };

    compRoot->AddChild(compList);

    compEmptyText = new TextElement();
    compEmptyText->text = "No components";
    compEmptyText->fontSize = 11.0f;
    compEmptyText->anchor = { 0.0f, 0.0f };
    compEmptyText->pivot = { 0.0f, 0.0f };
    compEmptyText->offset = { 8.0f, 0.0f };
    compEmptyText->size = { 0.0f, 16.0f };
    compRoot->AddChild(compEmptyText);
}

void StargraphEditorPanel::RenderComponentOutliner(Rectangle area) {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(area, theme.panelBg);
    DrawRectangleLinesEx(area, 1, theme.panelBorder);

    float y = area.y + 6;
    bool popupWasOpenThisFrame = addComponentMenu.IsOpen();

    DrawText("Components", (int)area.x + 8, (int)y, 12, theme.textSecondary);

    Rectangle addBtn = {area.x + area.width - 50, y - 2, 44, 16};
    bool addHov = !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(GetMousePosition(), addBtn);
    DrawRectangleRec(addBtn, addHov ? theme.buttonHover : theme.buttonBg);
    DrawRectangleLinesEx(addBtn, 1, theme.buttonBorder);
    int addTextW = MeasureText("+ Add", 10);
    DrawText("+ Add", (int)(addBtn.x + (addBtn.width - addTextW) / 2.0f),
             (int)(addBtn.y + (addBtn.height - 10) / 2.0f), 10, theme.textPrimary);
    if (addHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (addComponentMenu.IsOpen()) addComponentMenu.Close();
        else addComponentMenu.Open({addBtn.x, addBtn.y + addBtn.height});
    }

    y += 18;
    DrawLine((int)area.x + 4, (int)y, (int)(area.x + area.width - 4), (int)y, theme.panelBorder);
    y += 6;

    if (!compChrome) BuildComponentChrome();

    std::vector<ComponentDecl>& comps = graph.GetComponents();

    std::vector<TreeListElement::Entry> entries;
    entries.reserve(comps.size());
    for (size_t i = 0; i < comps.size(); i++) {
        TreeListElement::Entry entry;
        entry.id = comps[i].name;
        entry.label = comps[i].name + " (" + comps[i].componentType + ")";
        entry.parentId = comps[i].parentName;
        entry.iconColor = {90, 150, 210, 255};
        entry.isCurrent = (selectedComponentIndex == (int)i);
        entries.push_back(entry);
    }
    compList->SetEntries(entries);

    compList->backgroundColor = { 0, 0, 0, 0 };
    compList->rowColor = theme.textPrimary;
    compList->rowAltColor = { 0, 0, 0, 20 };
    compList->rowHoveredColor = theme.surfaceHover;
    compList->rowCurrentColor = theme.surfaceSelected;
    compList->rowCurrentTextColor = theme.textPrimary;
    compList->dropLineColor = theme.accent;

    float listHeight = area.y + area.height - 4 - y;
    compList->size = { 0.0f, comps.empty() ? 0.0f : listHeight };
    compList->visible = !comps.empty();
    compEmptyText->visible = comps.empty();
    compEmptyText->color = theme.textSecondary;

    compRoot->backgroundColor = { 0, 0, 0, 0 };
    Rectangle inner = { area.x + 4, y, area.width - 8, listHeight };
    compRoot->size = { inner.width, inner.height };

    float dt = GetFrameTime();
    compChrome->Tick(dt, inner, 1.0f);
    WidgetInput::ProcessInput(compChrome, "Editor", dt, GetMousePosition(), 1.0f);
    compChrome->Draw(inner, 1.0f);

    if (addComponentMenu.IsOpen()) {
        std::vector<SearchableListPopup::Entry> menuEntries;
        menuEntries.push_back({"Components", "SceneComponent", "SceneComponent"});
        for (const ComponentTypeInfo& typeInfo : ComponentRegistry::Get().GetAll()) {
            menuEntries.push_back({"Components", typeInfo.componentType, typeInfo.componentType});
        }

        std::string picked = addComponentMenu.Render(menuEntries, screenBounds);
        if (!picked.empty()) {
            ComponentDecl comp;
            comp.componentType = picked;
            comp.name          = picked;
            graph.AddComponent(comp);
            selectedComponentIndex = (int)graph.GetComponents().size() - 1;
            lastValidComponentIndex = selectedComponentIndex;
            ClearOtherLeftSelections(3);
            isDirty = true;
        }
    }

    if (!popupWasOpenThisFrame && selectedComponentIndex != -1 &&
        previewGizmoState.hoverAxis == -1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(GetMousePosition(), area)) {
        selectedComponentIndex = -1;
    }
}

static std::vector<std::string> GetValidParentOptions(const Stargraph& graph, size_t componentIndex) {
    const std::vector<ComponentDecl>& comps = graph.GetComponents();
    const ComponentDecl& self = comps[componentIndex];

    std::vector<std::string> excluded = {self.name};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const ComponentDecl& c : comps) {
            bool parentExcluded = std::find(excluded.begin(), excluded.end(), c.parentName) != excluded.end();
            bool alreadyExcluded = std::find(excluded.begin(), excluded.end(), c.name) != excluded.end();
            if (parentExcluded && !alreadyExcluded) {
                excluded.push_back(c.name);
                changed = true;
            }
        }
    }

    std::vector<std::string> options = {""};
    for (const ComponentDecl& c : comps) {
        if (std::find(excluded.begin(), excluded.end(), c.name) == excluded.end())
            options.push_back(c.name);
    }
    return options;
}

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

std::string StemOrNone(const std::string& path) {
    return path.empty() ? std::string("None") : std::filesystem::path(path).stem().string();
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

PropertyRow HeaderRow(const std::string& id, const std::string& text) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Header;
    row.label = text;
    return row;
}

PropertyRow ReadOnlyRow(const std::string& id, const std::string& label, const std::string& value) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::ReadOnlyText;
    row.label = label;
    row.stringValue = value;
    return row;
}

PropertyRow StringRow(const std::string& id, const std::string& label, const std::string& value,
                      std::function<void(const std::string&)> setter, const std::string& placeholder = "") {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::String;
    row.label = label;
    row.stringValue = value;
    row.commitOnly = true;
    row.placeholder = placeholder;
    row.setString = std::move(setter);
    return row;
}

PropertyRow EnumRow(const std::string& id, const std::string& label, const std::string& value,
                    std::vector<std::string> options, std::function<void(const std::string&)> setter) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Enum;
    row.label = label;
    row.stringValue = value;
    row.enumOptions = std::move(options);
    row.setString = std::move(setter);
    return row;
}

PropertyRow BoolRow(const std::string& id, const std::string& label, bool value, std::function<void(bool)> setter) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Bool;
    row.label = label;
    row.boolValue = value;
    row.setBool = std::move(setter);
    return row;
}

PropertyRow FloatRow(const std::string& id, const std::string& label, float value, bool integer,
                     std::function<void(float)> setter) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Float;
    row.label = label;
    row.floatValue = value;
    row.integer = integer;
    row.setFloat = std::move(setter);
    return row;
}

PropertyRow Vec3Row(const std::string& id, const std::string& label, float x, float y, float z,
                    std::function<void(float, float, float)> setter) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Vec3;
    row.label = label;
    row.vec3[0] = x;
    row.vec3[1] = y;
    row.vec3[2] = z;
    row.setVec3 = std::move(setter);
    return row;
}

PropertyRow AssetRow(const std::string& id, const std::string& label, const std::string& extension, const std::string& display) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Asset;
    row.label = label;
    row.assetExtension = extension;
    row.stringValue = display;
    return row;
}

PropertyRow ButtonRow(const std::string& id, const std::string& label, std::function<void()> onPressed) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Button;
    row.label = label;
    row.onButtonPressed = std::move(onPressed);
    return row;
}

PropertyRow CellsRow(const std::string& id, std::vector<PropertyCell> cells, float indent = 0.0f) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Cells;
    row.cells = std::move(cells);
    row.indent = indent;
    return row;
}

PropertyCell LabelCell(const std::string& text, float width) {
    PropertyCell cell;
    cell.kind = PropertyCellKind::Label;
    cell.text = text;
    cell.width = width;
    return cell;
}

PropertyCell ButtonCell(const std::string& text, float width, std::function<void(Rectangle)> onPressed, Color color = { 0, 0, 0, 0 }) {
    PropertyCell cell;
    cell.kind = PropertyCellKind::Button;
    cell.text = text;
    cell.width = width;
    cell.color = color;
    cell.onPressed = std::move(onPressed);
    return cell;
}

PropertyCell NumberCell(float value, bool integer, float width, std::function<void(float)> onNumber) {
    PropertyCell cell;
    cell.kind = PropertyCellKind::Number;
    cell.number = value;
    cell.integer = integer;
    cell.width = width;
    cell.onNumber = std::move(onNumber);
    return cell;
}

PropertyCell TextCell(const std::string& text, float width, const std::string& placeholder, std::function<void(const std::string&)> onText) {
    PropertyCell cell;
    cell.kind = PropertyCellKind::Text;
    cell.text = text;
    cell.width = width;
    cell.placeholder = placeholder;
    cell.onText = std::move(onText);
    return cell;
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

ComponentDecl* StargraphEditorPanel::DetailComponent(int index) {
    std::vector<ComponentDecl>& comps = graph.GetComponents();
    return (index >= 0 && index < (int)comps.size()) ? &comps[index] : nullptr;
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
    inputMappingPicker.Close();
    inputKeyPicker.Close();
    inputDevicePicker.Close();
    activeDeviceFieldKey.clear();
    inputKeyPickerMappingIndex = -1;
    inputKeyPickerBindingIndex = -1;
    listeningForInputBindingMappingIndex = -1;
    listeningForInputBindingIndex = -1;
    saveTargetKind = 0;
    saveTargetIndex = -1;
    saveTargetFieldKey.clear();
}

void StargraphEditorPanel::BuildComponentRows(std::vector<PropertyRow>& rows, int idx) {
    const UIPalette& theme = UITheme::Get();
    ComponentDecl& comp = graph.GetComponents()[idx];

    rows.push_back(HeaderRow("comp.type", comp.componentType));
    rows.push_back(StringRow("comp.name", "Name", comp.name, [this, idx](const std::string& value) {
        if (ComponentDecl* c = DetailComponent(idx)) {
            std::string oldName = c->name;
            if (oldName != value) {
                for (ComponentDecl& other : graph.GetComponents()) {
                    if (other.parentName == oldName) other.parentName = value;
                }
            }
            c->name = value;
            isDirty = true;
        }
    }));

    std::string parentLabel = comp.parentName.empty() ? std::string("None (Root)") : comp.parentName;
    rows.push_back(CellsRow("comp.parent", {
        LabelCell("Parent", 90.0f),
        ButtonCell(parentLabel, 0.0f, [this, idx](Rectangle) {
            ComponentDecl* c = DetailComponent(idx);
            if (!c) return;
            std::vector<std::string> options = GetValidParentOptions(graph, (size_t)idx);
            if (options.empty()) return;
            auto it = std::find(options.begin(), options.end(), c->parentName);
            size_t next = (it != options.end()) ? (size_t)(it - options.begin()) : 0;
            next = (next + 1) % options.size();
            c->parentName = options[next];
            isDirty = true;
        })
    }));

    auto transformRow = [&](const std::string& id, const char* label, const char* saveKey, float x, float y, float z,
                            std::function<void(float, float, float)> setter) {
        PropertyRow row = Vec3Row(id, label, x, y, z, std::move(setter));
        bool has = comp.savableFields.count(saveKey) && !comp.savableFields[saveKey].empty();
        row.trailingLabel = "S";
        row.trailingColor = has ? theme.success : theme.fieldBg;
        std::string key = saveKey;
        row.onTrailingPressed = [this, idx, key](Rectangle rect) {
            saveTargetKind = 0;
            saveTargetIndex = idx;
            saveTargetFieldKey = key;
            savePicker.Open({ rect.x, rect.y + rect.height }, SaveFileRegistry::Category());
        };
        rows.push_back(std::move(row));
    };

    transformRow("comp.pos", "Position", "Position", comp.posX, comp.posY, comp.posZ, [this, idx](float x, float y, float z) {
        if (ComponentDecl* c = DetailComponent(idx)) { c->posX = x; c->posY = y; c->posZ = z; isDirty = true; }
    });
    transformRow("comp.rot", "Rotation", "Rotation", comp.rotX, comp.rotY, comp.rotZ, [this, idx](float x, float y, float z) {
        if (ComponentDecl* c = DetailComponent(idx)) { c->rotX = x; c->rotY = y; c->rotZ = z; isDirty = true; }
    });
    transformRow("comp.scale", "Scale", "Scale", comp.scaleX, comp.scaleY, comp.scaleZ, [this, idx](float x, float y, float z) {
        if (ComponentDecl* c = DetailComponent(idx)) { c->scaleX = x; c->scaleY = y; c->scaleZ = z; isDirty = true; }
    });

    const ComponentTypeInfo* typeInfo = ComponentRegistry::Get().Find(comp.componentType);
    if (typeInfo) {
        for (const ComponentFieldDesc& f : typeInfo->fields) {
            if (f.visibleIf && !f.visibleIf(comp)) continue;
            std::string key = f.key;
            std::string rowId = "f:" + key;
            std::string value = comp.Get(f.key, f.defaultValue);
            switch (f.kind) {
                case ComponentFieldKind::String:
                    rows.push_back(StringRow(rowId, f.label, value, [this, idx, key](const std::string& v) {
                        if (ComponentDecl* c = DetailComponent(idx)) { c->fields[key] = v; isDirty = true; }
                    }));
                    break;
                case ComponentFieldKind::Float:
                    rows.push_back(FloatRow(rowId, f.label, SafeParse::ToFloat(value), false, [this, idx, key](float v) {
                        if (ComponentDecl* c = DetailComponent(idx)) {
                            char buf[32];
                            snprintf(buf, sizeof(buf), "%g", v);
                            c->fields[key] = buf;
                            isDirty = true;
                        }
                    }));
                    break;
                case ComponentFieldKind::Bool:
                    rows.push_back(BoolRow(rowId, f.label, value == "true", [this, idx, key](bool v) {
                        if (ComponentDecl* c = DetailComponent(idx)) { c->fields[key] = v ? "true" : "false"; isDirty = true; }
                    }));
                    break;
                case ComponentFieldKind::Enum:
                    rows.push_back(EnumRow(rowId, f.label, value, f.enumOptions, [this, idx, key](const std::string& v) {
                        if (ComponentDecl* c = DetailComponent(idx)) { c->fields[key] = v; isDirty = true; }
                    }));
                    break;
                case ComponentFieldKind::AssetPath:
                    rows.push_back(AssetRow(rowId, f.label, f.assetExtension, StemOrNone(value)));
                    break;
                case ComponentFieldKind::Device:
                    rows.push_back(CellsRow(rowId, {
                        LabelCell(f.label, 90.0f),
                        ButtonCell(value, 0.0f, [this, key](Rectangle r) {
                            activeDeviceFieldKey = key;
                            inputDevicePicker.Open({ r.x, r.y + r.height }, r.width);
                        })
                    }));
                    break;
            }
        }
    }

    if (comp.componentType == "MeshComponent") {
        std::vector<MaterialSlot> meshSlots;
        std::string meshPath = comp.Get("Mesh");
        if (!meshPath.empty()) {
            StarAsset meshAsset;
            if (StarAsset::LoadFromFile(meshPath, meshAsset)) meshSlots = meshAsset.materialSlots;
        }
        std::vector<std::string>& materialPaths = comp.arrayFields["Material"];
        if (materialPaths.size() != meshSlots.size()) materialPaths.resize(meshSlots.size());
        if (meshSlots.empty()) {
            rows.push_back(ReadOnlyRow("mat.none", "Materials", "No material slots"));
        } else {
            rows.push_back(HeaderRow("mat.h", "Materials"));
            for (size_t i = 0; i < meshSlots.size(); i++) {
                std::string slotName = meshSlots[i].usemtlName.empty() ? ("Slot " + std::to_string(i)) : meshSlots[i].usemtlName;
                rows.push_back(AssetRow("mat:" + std::to_string(i), slotName, ".starmat", StemOrNone(materialPaths[i])));
            }
        }
    } else if (comp.componentType == "InputComponent") {
        static const char* mappingTypeNames[] = { "Bool", "Axis", "Axis2D" };
        std::vector<std::string>& mappingNames = comp.arrayFields["InputMapping"];

        auto mappingNameAt = [this](int componentIndex, int mappingIndex) -> std::string {
            ComponentDecl* c = DetailComponent(componentIndex);
            if (!c) return std::string();
            std::vector<std::string>& names = c->arrayFields["InputMapping"];
            return (mappingIndex >= 0 && mappingIndex < (int)names.size()) ? names[mappingIndex] : std::string();
        };
        auto loadMapping = [](const std::string& name) {
            InputMapping mapping;
            if (const InputMapping* existing = InputMappingRegistry::Get().Find(name)) mapping = *existing;
            return mapping;
        };

        rows.push_back(CellsRow("in.h", {
            LabelCell("Mappings", 0.0f),
            ButtonCell("+ Add", 60.0f, [this](Rectangle r) {
                inputMappingPicker.Open({ r.x, r.y + r.height }, InputMappingRegistry::Category());
            })
        }));

        for (size_t mi = 0; mi < mappingNames.size(); mi++) {
            int mIndex = (int)mi;
            std::string mappingName = mappingNames[mi];
            InputMapping mapping = loadMapping(mappingName);

            rows.push_back(CellsRow("in.m" + std::to_string(mi), {
                LabelCell(mappingName, 0.0f),
                ButtonCell(mappingTypeNames[(int)mapping.type], 52.0f, [this, idx, mIndex, mappingNameAt, loadMapping](Rectangle) {
                    std::string name = mappingNameAt(idx, mIndex);
                    if (name.empty()) return;
                    InputMapping m = loadMapping(name);
                    m.type = (InputMappingType)(((int)m.type + 1) % 3);
                    InputMappingRegistry::Get().SetMapping(name, m);
                }),
                ButtonCell("+ Key", 42.0f, [this, mIndex](Rectangle r) {
                    inputKeyPickerMappingIndex = mIndex;
                    inputKeyPickerBindingIndex = -1;
                    inputKeyPicker.Open({ r.x, r.y + r.height });
                }, theme.successBg),
                ButtonCell("x", 22.0f, [this, idx, mIndex](Rectangle) {
                    ComponentDecl* c = DetailComponent(idx);
                    if (!c) return;
                    std::vector<std::string>& names = c->arrayFields["InputMapping"];
                    if (mIndex >= 0 && mIndex < (int)names.size()) {
                        names.erase(names.begin() + mIndex);
                        isDirty = true;
                    }
                }, theme.dangerBg)
            }));

            for (size_t bi = 0; bi < mapping.bindings.size(); bi++) {
                int bIndex = (int)bi;
                const InputMappingBinding& binding = mapping.bindings[bi];
                std::string display;
                if (binding.requireCtrl) display += "Ctrl + ";
                if (binding.requireAlt) display += "Alt + ";
                if (binding.requireShift) display += "Shift + ";
                for (char c : binding.key) display += (c == '_') ? ' ' : c;
                bool listening = (listeningForInputBindingMappingIndex == mIndex && listeningForInputBindingIndex == bIndex);

                auto toggleModifier = [this, idx, mIndex, bIndex, mappingNameAt, loadMapping](bool InputMappingBinding::* field) {
                    std::string name = mappingNameAt(idx, mIndex);
                    if (name.empty()) return;
                    InputMapping m = loadMapping(name);
                    if (bIndex < 0 || bIndex >= (int)m.bindings.size()) return;
                    m.bindings[bIndex].*field = !(m.bindings[bIndex].*field);
                    InputMappingRegistry::Get().SetMapping(name, m);
                };

                std::vector<PropertyCell> cells;
                cells.push_back(ButtonCell(display, 0.0f, [this, mIndex, bIndex](Rectangle r) {
                    inputKeyPickerMappingIndex = mIndex;
                    inputKeyPickerBindingIndex = bIndex;
                    inputKeyPicker.Open({ r.x, r.y + r.height });
                }));
                cells.push_back(ButtonCell("Ctrl", 34.0f, [toggleModifier](Rectangle) {
                    toggleModifier(&InputMappingBinding::requireCtrl);
                }, binding.requireCtrl ? theme.successBg : Color{ 0, 0, 0, 0 }));
                cells.push_back(ButtonCell("Alt", 30.0f, [toggleModifier](Rectangle) {
                    toggleModifier(&InputMappingBinding::requireAlt);
                }, binding.requireAlt ? theme.successBg : Color{ 0, 0, 0, 0 }));
                cells.push_back(ButtonCell("Shift", 38.0f, [toggleModifier](Rectangle) {
                    toggleModifier(&InputMappingBinding::requireShift);
                }, binding.requireShift ? theme.successBg : Color{ 0, 0, 0, 0 }));
                if (mapping.type != InputMappingType::Bool) {
                    cells.push_back(NumberCell(binding.scale, false, 40.0f, [this, idx, mIndex, bIndex, mappingNameAt, loadMapping](float v) {
                        std::string name = mappingNameAt(idx, mIndex);
                        if (name.empty()) return;
                        InputMapping m = loadMapping(name);
                        if (bIndex < 0 || bIndex >= (int)m.bindings.size()) return;
                        m.bindings[bIndex].scale = v;
                        InputMappingRegistry::Get().SetMapping(name, m);
                    }));
                }
                if (mapping.type == InputMappingType::Axis2D) {
                    cells.push_back(ButtonCell(binding.axis == 0 ? "X" : "Y", 24.0f, [this, idx, mIndex, bIndex, mappingNameAt, loadMapping](Rectangle) {
                        std::string name = mappingNameAt(idx, mIndex);
                        if (name.empty()) return;
                        InputMapping m = loadMapping(name);
                        if (bIndex < 0 || bIndex >= (int)m.bindings.size()) return;
                        m.bindings[bIndex].axis = (m.bindings[bIndex].axis == 0) ? 1 : 0;
                        InputMappingRegistry::Get().SetMapping(name, m);
                    }));
                }
                cells.push_back(ButtonCell(listening ? "?" : "...", 22.0f, [this, mIndex, bIndex](Rectangle) {
                    listeningForInputBindingMappingIndex = mIndex;
                    listeningForInputBindingIndex = bIndex;
                    inputKeyPicker.Close();
                }, listening ? Color{ 160, 90, 30, 255 } : Color{ 0, 0, 0, 0 }));
                cells.push_back(ButtonCell("x", 20.0f, [this, idx, mIndex, bIndex, mappingNameAt, loadMapping](Rectangle) {
                    std::string name = mappingNameAt(idx, mIndex);
                    if (name.empty()) return;
                    InputMapping m = loadMapping(name);
                    if (bIndex < 0 || bIndex >= (int)m.bindings.size()) return;
                    m.bindings.erase(m.bindings.begin() + bIndex);
                    InputMappingRegistry::Get().SetMapping(name, m);
                }, theme.dangerBg));

                rows.push_back(CellsRow("in.b" + std::to_string(mi) + "." + std::to_string(bi), std::move(cells), 8.0f));
            }
        }
    }
}

void StargraphEditorPanel::RenderDetailPickers(Rectangle panel) {
    ComponentDecl* comp = DetailComponent(lastValidComponentIndex);
    if (comp) {
        if (!activeDeviceFieldKey.empty() && inputDevicePicker.IsOpen()) {
            std::vector<SearchableListPopup::Entry> deviceEntries;
            deviceEntries.push_back({ "Devices", "Unclaimed", "Unclaimed" });
            deviceEntries.push_back({ "Devices", "Keyboard+Mouse", "Keyboard+Mouse" });
            for (int i = 0; i < 4; i++) {
                if (!IsGamepadAvailable(i)) continue;
                std::string name = "Gamepad " + std::to_string(i);
                deviceEntries.push_back({ "Devices", name, name });
            }
            std::string pickedDevice = inputDevicePicker.Render(deviceEntries, panel);
            if (!pickedDevice.empty()) {
                comp->fields[activeDeviceFieldKey] = pickedDevice;
                isDirty = true;
            }
        }

        if (comp->componentType == "InputComponent") {
            std::vector<std::string>& mappingNames = comp->arrayFields["InputMapping"];

            std::string pickedMappingName;
            if (inputMappingPicker.Render(pickedMappingName) && !pickedMappingName.empty()) {
                bool alreadyOwned = std::find(mappingNames.begin(), mappingNames.end(), pickedMappingName) != mappingNames.end();
                if (!alreadyOwned) {
                    if (!InputMappingRegistry::Get().Find(pickedMappingName)) InputMappingRegistry::Get().SetMapping(pickedMappingName, InputMapping{});
                    mappingNames.push_back(pickedMappingName);
                    isDirty = true;
                }
            }

            if (inputKeyPicker.IsOpen()) {
                std::vector<SearchableListPopup::Entry> keyEntries;
                for (const std::string& keyName : GetInputKeyNames()) {
                    if (InputKeyIsAnalog(keyName)) continue;
                    std::string category = "Keyboard";
                    if (keyName.rfind("GAMEPAD_", 0) == 0) category = "Gamepad";
                    else if (keyName.find("MOUSE") != std::string::npos) category = "Mouse";
                    std::string label = keyName;
                    for (char& c : label) if (c == '_') c = ' ';
                    keyEntries.push_back({ category, keyName, label });
                }
                std::string pickedKey = inputKeyPicker.Render(keyEntries, panel);
                if (!pickedKey.empty() && inputKeyPickerMappingIndex >= 0 && inputKeyPickerMappingIndex < (int)mappingNames.size()) {
                    const std::string& mappingName = mappingNames[inputKeyPickerMappingIndex];
                    InputMapping mapping;
                    if (const InputMapping* existing = InputMappingRegistry::Get().Find(mappingName)) mapping = *existing;
                    if (inputKeyPickerBindingIndex >= 0 && inputKeyPickerBindingIndex < (int)mapping.bindings.size()) {
                        mapping.bindings[inputKeyPickerBindingIndex].key = pickedKey;
                    } else {
                        InputMappingBinding newBinding;
                        newBinding.key = pickedKey;
                        mapping.bindings.push_back(newBinding);
                    }
                    InputMappingRegistry::Get().SetMapping(mappingName, mapping);
                }
            }

            if (listeningForInputBindingMappingIndex != -1) {
                WidgetInput::MarkKeyboardCaptured();
                if (IsKeyPressed(KEY_ESCAPE)) {
                    listeningForInputBindingMappingIndex = -1;
                    listeningForInputBindingIndex = -1;
                } else {
                    int keyCode = GetKeyPressed();
                    while (keyCode != 0 && InputKeyCodeIsModifier(keyCode)) keyCode = GetKeyPressed();
                    if (keyCode != 0) {
                        std::string keyName = InputKeyCodeToName(keyCode);
                        if (!keyName.empty() && listeningForInputBindingMappingIndex < (int)mappingNames.size()) {
                            const std::string& mappingName = mappingNames[listeningForInputBindingMappingIndex];
                            InputMapping mapping;
                            if (const InputMapping* existing = InputMappingRegistry::Get().Find(mappingName)) mapping = *existing;
                            if (listeningForInputBindingIndex >= 0 && listeningForInputBindingIndex < (int)mapping.bindings.size()) {
                                mapping.bindings[listeningForInputBindingIndex].key = keyName;
                                mapping.bindings[listeningForInputBindingIndex].requireCtrl = IsCtrlKeyDown();
                                mapping.bindings[listeningForInputBindingIndex].requireAlt = IsAltKeyDown();
                                mapping.bindings[listeningForInputBindingIndex].requireShift = IsShiftKeyDown();
                                InputMappingRegistry::Get().SetMapping(mappingName, mapping);
                            }
                        }
                        listeningForInputBindingMappingIndex = -1;
                        listeningForInputBindingIndex = -1;
                    }
                }
            }
        }
    }

    std::string pickedSaveFile;
    if (savePicker.Render(pickedSaveFile)) {
        if (saveTargetKind == 1) {
            if (Variable* v = DetailVariable(saveTargetIndex, false)) {
                v->saveFile = pickedSaveFile;
                isDirty = true;
            }
        } else if (ComponentDecl* c = DetailComponent(saveTargetIndex)) {
            if (pickedSaveFile.empty()) c->savableFields.erase(saveTargetFieldKey);
            else c->savableFields[saveTargetFieldKey] = pickedSaveFile;
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
            saveTargetKind = 1;
            saveTargetIndex = idx;
            saveTargetFieldKey.clear();
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
    for (const Pin& pin : node->inputs) {
        if (pin.type == PinType::Exec) continue;
        if (pin.name == "Variable_Name" || pin.name == "Component_Name") continue;
        std::string value = pin.defaultValue.c_str();
        if ((node->type == "Play_Sound" || node->type == "Load_Music") && pin.name == "Path") {
            rows.push_back(AssetRow("audio:" + std::to_string(pin.id), pin.name, ".starsound", StemOrNone(value)));
        } else {
            rows.push_back(ReadOnlyRow("n.p" + std::to_string(pin.id), pin.name, value.empty() ? std::string("(empty)") : value));
        }
    }
}

std::vector<PropertyRow> StargraphEditorPanel::BuildDetailsRows() {
    std::vector<PropertyRow> rows;

    std::vector<ComponentDecl>& comps = graph.GetComponents();
    if (lastValidComponentIndex >= (int)comps.size()) lastValidComponentIndex = -1;
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

    std::string key;
    if (showSelfActorDetails) key = "self";
    else if (lastValidComponentIndex >= 0) key = "c" + std::to_string(lastValidComponentIndex);
    else if (lastValidVariableIndex >= 0) key = "v" + std::to_string(lastValidVariableIndex);
    else if (lastValidFunctionIndex >= 0) key = "f" + std::to_string(lastValidFunctionIndex);
    else if (localVariableSelected) key = "l" + std::to_string(lastValidLocalVariableIndex);
    else if (selectedNode) key = "n" + std::to_string(selectedNodeId) + ":" + selectedNode->type;

    if (key != lastDetailsSelectionKey) {
        ResetDetailPickers();
        lastDetailsSelectionKey = key;
    }

    if (showSelfActorDetails) {
        rows.push_back(HeaderRow("self.h", graph.assetName));
        rows.push_back(ReadOnlyRow("self.d", "Type", "Actor properties"));
    } else if (lastValidComponentIndex >= 0) {
        BuildComponentRows(rows, lastValidComponentIndex);
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
    if (rowId.rfind("f:", 0) == 0) {
        if (ComponentDecl* c = DetailComponent(lastValidComponentIndex)) {
            c->fields[rowId.substr(2)] = pickedPath;
            isDirty = true;
        }
    } else if (rowId.rfind("mat:", 0) == 0) {
        if (ComponentDecl* c = DetailComponent(lastValidComponentIndex)) {
            std::vector<std::string>& mats = c->arrayFields["Material"];
            int slot = atoi(rowId.c_str() + 4);
            if (slot >= 0 && slot < (int)mats.size()) {
                mats[slot] = pickedPath;
                isDirty = true;
            }
        }
    } else if (rowId.rfind("audio:", 0) == 0) {
        Pin* pin = ActiveGraph().FindPin(atoi(rowId.c_str() + 6));
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
