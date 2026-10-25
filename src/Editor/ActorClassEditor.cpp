#include "ActorClassEditor.h"
#include "Editor/PropertyRowBuilders.h"
#include "Runtime/ComponentRegistry.h"
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
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace PropertyRows;

ActorClassEditor::ActorClassEditor() = default;

void ActorClassEditor::SetHost(ActorClassEditorHost newHost) {
    host = std::move(newHost);
}

std::vector<ComponentDecl>& ActorClassEditor::Components() {
    static std::vector<ComponentDecl> empty;
    if (!host.components) {
        empty.clear();
        return empty;
    }
    return host.components();
}

void ActorClassEditor::MarkDirty() {
    if (host.markDirty) host.markDirty();
}

void ActorClassEditor::NotifyComponentSelected() {
    if (host.onComponentSelected) host.onComponentSelected();
}

void ActorClassEditor::SelectComponent(int index) {
    selectedComponentIndex = index;
    lastValidComponentIndex = index;
}

void ActorClassEditor::ClearSelection() {
    selectedComponentIndex = -1;
    lastValidComponentIndex = -1;
}

void ActorClassEditor::ClampSelection() {
    int count = (int)Components().size();
    if (selectedComponentIndex >= count) selectedComponentIndex = -1;
    if (lastValidComponentIndex >= count) lastValidComponentIndex = -1;
}

void ActorClassEditor::OnDocumentReplaced() {
    liveComponents.Clear();
    ClampSelection();
    ResetPickers();
}

std::string ActorClassEditor::DetailsSelectionKey() {
    if (lastValidComponentIndex >= 0 && lastValidComponentIndex < (int)Components().size())
        return "c" + std::to_string(lastValidComponentIndex);
    return std::string();
}

bool ActorClassEditor::HasDetailsSelection() {
    return lastValidComponentIndex >= 0 && lastValidComponentIndex < (int)Components().size();
}

void ActorClassEditor::BuildDetailsRows(std::vector<PropertyRow>& rows) {
    if (!HasDetailsSelection()) return;
    BuildComponentRows(rows, lastValidComponentIndex);
}

void ActorClassEditor::ResetPickers() {
    savePicker.Close();
    inputMappingPicker.Close();
    inputKeyPicker.Close();
    inputDevicePicker.Close();
    activeDeviceFieldKey.clear();
    inputKeyPickerMappingIndex = -1;
    inputKeyPickerBindingIndex = -1;
    listeningForInputBindingMappingIndex = -1;
    listeningForInputBindingIndex = -1;
    saveTargetIndex = -1;
    saveTargetFieldKey.clear();
}

void ActorClassEditor::BuildOutlinerChrome() {
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
        std::vector<ComponentDecl>& comps = Components();
        for (size_t i = 0; i < comps.size(); i++) {
            if (comps[i].name == id) {
                selectedComponentIndex = (int)i;
                lastValidComponentIndex = (int)i;
                break;
            }
        }
        NotifyComponentSelected();
    };

    compList->onDeleteRequested = [this](const std::string& id) {
        std::vector<ComponentDecl>& comps = Components();
        for (size_t i = 0; i < comps.size(); i++) {
            if (comps[i].name != id) continue;
            for (ComponentDecl& child : comps) {
                if (child.parentName == id) child.parentName = comps[i].parentName;
            }
            comps.erase(comps.begin() + (long)i);
            selectedComponentIndex = -1;
            lastValidComponentIndex = -1;
            MarkDirty();
            break;
        }
    };

    compList->onReorderOrReparent = [this](const std::string& draggedId, const std::string& targetId, TreeDropPosition pos) {
        std::vector<ComponentDecl>& comps = Components();
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
        MarkDirty();
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

void ActorClassEditor::RenderOutliner(Rectangle area, Rectangle screenBounds) {
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

    if (!compChrome) BuildOutlinerChrome();

    std::vector<ComponentDecl>& comps = Components();

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
            Components().push_back(comp);
            selectedComponentIndex = (int)Components().size() - 1;
            lastValidComponentIndex = selectedComponentIndex;
            NotifyComponentSelected();
            MarkDirty();
        }
    }

    if (!popupWasOpenThisFrame && selectedComponentIndex != -1 &&
        previewGizmoState.hoverAxis == -1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(GetMousePosition(), area)) {
        selectedComponentIndex = -1;
    }
}


static std::vector<std::string> GetValidParentOptions(const std::vector<ComponentDecl>& comps, size_t componentIndex) {
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


ComponentDecl* ActorClassEditor::DetailComponent(int index) {
    std::vector<ComponentDecl>& comps = Components();
    return (index >= 0 && index < (int)comps.size()) ? &comps[index] : nullptr;
}

void ActorClassEditor::BuildComponentRows(std::vector<PropertyRow>& rows, int idx) {
    const UIPalette& theme = UITheme::Get();
    ComponentDecl& comp = Components()[idx];

    rows.push_back(HeaderRow("comp.type", comp.componentType));
    rows.push_back(StringRow("comp.name", "Name", comp.name, [this, idx](const std::string& value) {
        if (ComponentDecl* c = DetailComponent(idx)) {
            std::string oldName = c->name;
            if (oldName != value) {
                for (ComponentDecl& other : Components()) {
                    if (other.parentName == oldName) other.parentName = value;
                }
            }
            c->name = value;
            MarkDirty();
        }
    }));

    std::string parentLabel = comp.parentName.empty() ? std::string("None (Root)") : comp.parentName;
    rows.push_back(CellsRow("comp.parent", {
        LabelCell("Parent", 90.0f),
        ButtonCell(parentLabel, 0.0f, [this, idx](Rectangle) {
            ComponentDecl* c = DetailComponent(idx);
            if (!c) return;
            std::vector<std::string> options = GetValidParentOptions(Components(), (size_t)idx);
            if (options.empty()) return;
            auto it = std::find(options.begin(), options.end(), c->parentName);
            size_t next = (it != options.end()) ? (size_t)(it - options.begin()) : 0;
            next = (next + 1) % options.size();
            c->parentName = options[next];
            MarkDirty();
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
            saveTargetIndex = idx;
            saveTargetFieldKey = key;
            savePicker.Open({ rect.x, rect.y + rect.height }, SaveFileRegistry::Category());
        };
        rows.push_back(std::move(row));
    };

    transformRow("comp.pos", "Position", "Position", comp.posX, comp.posY, comp.posZ, [this, idx](float x, float y, float z) {
        if (ComponentDecl* c = DetailComponent(idx)) { c->posX = x; c->posY = y; c->posZ = z; MarkDirty(); }
    });
    transformRow("comp.rot", "Rotation", "Rotation", comp.rotX, comp.rotY, comp.rotZ, [this, idx](float x, float y, float z) {
        if (ComponentDecl* c = DetailComponent(idx)) { c->rotX = x; c->rotY = y; c->rotZ = z; MarkDirty(); }
    });
    transformRow("comp.scale", "Scale", "Scale", comp.scaleX, comp.scaleY, comp.scaleZ, [this, idx](float x, float y, float z) {
        if (ComponentDecl* c = DetailComponent(idx)) { c->scaleX = x; c->scaleY = y; c->scaleZ = z; MarkDirty(); }
    });

    if (ComponentRegistry::Get().TypeHasState(comp.componentType)) {
        auto stateIt = comp.savableFields.find("@State");
        std::string stateFile = stateIt == comp.savableFields.end() ? std::string() : stateIt->second;
        rows.push_back(CellsRow("comp.state", {
            LabelCell("State Save File", 110.0f),
            ButtonCell(stateFile.empty() ? std::string("None") : stateFile, 0.0f, [this, idx](Rectangle r) {
                saveTargetIndex = idx;
                saveTargetFieldKey = "@State";
                savePicker.Open({ r.x, r.y + r.height }, SaveFileRegistry::Category());
            })
        }));
    }

    const ComponentTypeInfo* typeInfo = ComponentRegistry::Get().Find(comp.componentType);
    if (typeInfo) {
        for (const ComponentFieldDesc& f : typeInfo->fields) {
            if (f.visibleIf && !f.visibleIf(comp)) continue;
            if (f.hiddenInDetails) continue;
            std::string key = f.key;
            std::string rowId = "f:" + key;
            std::string value = ComponentEditing::ReadField(liveComponents, comp.name, comp, f);
            if (f.colorKeys.size() == 3) {
                PropertyRow colorRow;
                colorRow.id = rowId;
                colorRow.kind = PropertyRowKind::Color;
                colorRow.label = f.label;
                colorRow.colorHasAlpha = false;
                int bytes[3] = { 255, 255, 255 };
                for (int c = 0; c < 3; c++) {
                    for (const ComponentFieldDesc& channel : typeInfo->fields) {
                        if (channel.key == f.colorKeys[c]) {
                            bytes[c] = ComponentField::ChannelToByte(SafeParse::ToFloat(ComponentEditing::ReadField(liveComponents, comp.name, comp, channel)));
                        }
                    }
                }
                colorRow.colorValue = Color{ (unsigned char)bytes[0], (unsigned char)bytes[1], (unsigned char)bytes[2], 255 };
                std::vector<std::string> keys = f.colorKeys;
                colorRow.setColor = [this, idx, keys](Color picked) {
                    SetComponentField(idx, keys[0], ComponentField::FloatStr(ComponentField::ByteToChannel(picked.r)));
                    SetComponentField(idx, keys[1], ComponentField::FloatStr(ComponentField::ByteToChannel(picked.g)));
                    SetComponentField(idx, keys[2], ComponentField::FloatStr(ComponentField::ByteToChannel(picked.b)));
                };
                rows.push_back(std::move(colorRow));
                continue;
            }
            switch (f.kind) {
                case ComponentFieldKind::String:
                    rows.push_back(StringRow(rowId, f.label, value, [this, idx, key](const std::string& v) {
                        SetComponentField(idx, key, v);
                    }));
                    break;
                case ComponentFieldKind::Float:
                    rows.push_back(FloatRow(rowId, f.label, SafeParse::ToFloat(value), false, [this, idx, key](float v) {
                        SetComponentField(idx, key, ComponentField::FloatStr(v));
                    }));
                    break;
                case ComponentFieldKind::Bool:
                    rows.push_back(BoolRow(rowId, f.label, value == "true", [this, idx, key](bool v) {
                        SetComponentField(idx, key, ComponentField::BoolStr(v));
                    }));
                    break;
                case ComponentFieldKind::Enum:
                    rows.push_back(EnumRow(rowId, f.label, value, ComponentField::EnumOptionsOf(f), [this, idx, key](const std::string& v) {
                        SetComponentField(idx, key, v);
                    }));
                    break;
                case ComponentFieldKind::LayerMask: {
                    uint32_t maskValue = CollisionLayers::ParseMask(value);
                    rows.push_back(HeaderRow(rowId + ".h", f.label));
                    for (int layerIndex : CollisionLayers::UsedIndices()) {
                        bool enabled = (maskValue & CollisionLayers::Bit(layerIndex)) != 0;
                        rows.push_back(BoolRow(rowId + "." + std::to_string(layerIndex), CollisionLayers::NameAt(layerIndex), enabled,
                            [this, idx, key, maskValue, layerIndex](bool v) {
                                uint32_t next = v ? (maskValue | CollisionLayers::Bit(layerIndex)) : (maskValue & ~CollisionLayers::Bit(layerIndex));
                                SetComponentField(idx, key, CollisionLayers::MaskToString(next));
                            }));
                    }
                    break;
                }
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
                        MarkDirty();
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

void ActorClassEditor::RenderPickers(Rectangle panel) {
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
                ComponentEditing::WriteField(liveComponents, comp->name, *comp, activeDeviceFieldKey, pickedDevice);
                MarkDirty();
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
                    MarkDirty();
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
        if (ComponentDecl* c = DetailComponent(saveTargetIndex)) {
            if (pickedSaveFile.empty()) c->savableFields.erase(saveTargetFieldKey);
            else c->savableFields[saveTargetFieldKey] = pickedSaveFile;
            MarkDirty();
        }
    }
}

void ActorClassEditor::SetComponentField(int index, const std::string& fieldKey, const std::string& value) {
    ComponentDecl* c = DetailComponent(index);
    if (!c) return;
    ComponentEditing::WriteField(liveComponents, c->name, *c, fieldKey, value);
    MarkDirty();
}

bool ActorClassEditor::OnAssetPicked(const std::string& rowId, const std::string& pickedPath) {
    if (rowId.rfind("f:", 0) == 0) {
        if (ComponentDecl* c = DetailComponent(lastValidComponentIndex)) {
            ComponentEditing::WriteField(liveComponents, c->name, *c, rowId.substr(2), pickedPath);
            MarkDirty();
        }
        return true;
    }
    if (rowId.rfind("mat:", 0) == 0) {
        if (ComponentDecl* c = DetailComponent(lastValidComponentIndex)) {
            std::vector<std::string> mats = c->arrayFields["Material"];
            int slot = atoi(rowId.c_str() + 4);
            if (slot >= 0 && slot < (int)mats.size()) {
                mats[slot] = pickedPath;
                ComponentEditing::WriteArray(liveComponents, c->name, *c, "Material", mats);
                MarkDirty();
            }
        }
        return true;
    }
    return false;
}
