#include "PropertyPanel.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/VerticalBoxElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/CheckBoxElement.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

class LabeledRow : public WidgetElement {
public:
    float labelWidth = 90.0f;
    bool hasTrailing = false;
    float trailingWidth = 20.0f;

protected:
    Rectangle ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const override {
        float lw = labelWidth * uiScale;
        float tw = trailingWidth * uiScale;
        float gap = 4.0f * uiScale;
        if (hasTrailing && childIndex == 2) return { ownRect.x + ownRect.width - tw, ownRect.y, tw, ownRect.height };
        if (childIndex == 0) return { ownRect.x, ownRect.y, lw, ownRect.height };
        float controlX = ownRect.x + lw;
        float controlW = ownRect.width - lw - (hasTrailing ? tw + gap : 0.0f);
        if (child->size.x > 0.0f) {
            return { controlX, ownRect.y, child->size.x * uiScale, ownRect.height };
        }
        return { controlX, ownRect.y, controlW, ownRect.height };
    }
};

class TripletRow : public WidgetElement {
public:
    int count = 3;
    bool hasTrailing = false;
    float trailingWidth = 20.0f;

protected:
    Rectangle ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const override {
        float gap = 4.0f * uiScale;
        float tw = trailingWidth * uiScale;
        if (hasTrailing && childIndex == count * 2) return { ownRect.x + ownRect.width - tw, ownRect.y, tw, ownRect.height };
        float usable = ownRect.width - (hasTrailing ? tw + gap : 0.0f);
        float slotW = (usable - gap * (float)(count - 1)) / (float)count;
        int slot = childIndex / 2;
        bool isLabel = (childIndex % 2) == 0;
        float slotX = ownRect.x + (float)slot * (slotW + gap);
        float letterW = 12.0f * uiScale;
        if (isLabel) return { slotX, ownRect.y, letterW, ownRect.height };
        return { slotX + letterW, ownRect.y, slotW - letterW, ownRect.height };
    }
};

class CellRow : public WidgetElement {
public:
    float indent = 0.0f;

protected:
    Rectangle ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const override {
        float gap = 4.0f * uiScale;
        int n = (int)children.size();
        float fixedTotal = 0.0f;
        int flexCount = 0;
        for (WidgetElement* c : children) {
            if (c->size.x > 0.0f) fixedTotal += c->size.x * uiScale;
            else flexCount++;
        }
        float available = ownRect.width - indent * uiScale - gap * (float)(n > 0 ? n - 1 : 0) - fixedTotal;
        float flexW = flexCount > 0 ? std::max(0.0f, available) / (float)flexCount : 0.0f;
        float x = ownRect.x + indent * uiScale;
        for (int i = 0; i < childIndex; i++) {
            float w = children[i]->size.x > 0.0f ? children[i]->size.x * uiScale : flexW;
            x += w + gap;
        }
        float w = child->size.x > 0.0f ? child->size.x * uiScale : flexW;
        return { x, ownRect.y, w, ownRect.height };
    }
};

bool RowsShapeEqual(const std::vector<PropertyRow>& a, const std::vector<PropertyRow>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].id != b[i].id || a[i].kind != b[i].kind || a[i].label != b[i].label ||
            a[i].enumOptions != b[i].enumOptions || a[i].indent != b[i].indent ||
            a[i].trailingLabel.empty() != b[i].trailingLabel.empty() || a[i].integer != b[i].integer ||
            a[i].commitOnly != b[i].commitOnly || a[i].cells.size() != b[i].cells.size()) {
            return false;
        }
        for (size_t c = 0; c < a[i].cells.size(); c++) {
            if (a[i].cells[c].kind != b[i].cells[c].kind || a[i].cells[c].width != b[i].cells[c].width) return false;
        }
    }
    return true;
}

std::string FormatInteger(float v) {
    return std::to_string((int)std::lround(v));
}

std::string FormatFloat(float v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", v);
    std::string s = buf;
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        size_t last = s.find_last_not_of('0');
        if (last == dot) last--;
        s.erase(last + 1);
    }
    return s;
}

}

PropertyPanel::PropertyPanel() {
    root = new VerticalBoxElement();
    root->scrollEnabled = true;
    root->childSpacing = 2.0f;
}

WidgetElement* PropertyPanel::GetRootElement() const {
    return root;
}

void PropertyPanel::SetProperties(std::vector<PropertyRow> newRows) {
    bool sameShape = RowsShapeEqual(rows, newRows);
    rows = std::move(newRows);
    if (sameShape) {
        RefreshValues();
    } else {
        RebuildRows();
    }
}

void PropertyPanel::RebuildRows() {
    root->ClearChildren();
    rowWidgets.clear();
    rowWidgets.resize(rows.size());

    auto addTrailing = [this](WidgetElement* rowElement, RowWidgets& rw, size_t i, const PropertyRow& row) {
        auto* btn = new ButtonElement();
        btn->size = { 20.0f, rowHeight };
        btn->defaultColor = row.trailingColor.a > 0 ? row.trailingColor : buttonBg;
        btn->hoveredColor = buttonHover;
        btn->selectedColor = buttonHover;
        btn->pressedColor = buttonHover;
        btn->onActivated = [this, i]() {
            if (i >= rows.size() || i >= rowWidgets.size()) return;
            if (rows[i].onTrailingPressed && rowWidgets[i].trailingButton) {
                rows[i].onTrailingPressed(rowWidgets[i].trailingButton->lastResolvedRect);
            }
        };
        auto* lbl = new TextElement();
        lbl->text = row.trailingLabel;
        lbl->fontSize = 10.0f;
        lbl->anchor = { 0.5f, 0.5f };
        lbl->pivot = { 0.5f, 0.5f };
        btn->AddChild(lbl);
        rowElement->AddChild(btn);
        rw.trailingButton = btn;
        rw.trailingLabel = lbl;
    };

    for (size_t i = 0; i < rows.size(); i++) {
        const PropertyRow& row = rows[i];
        RowWidgets& rw = rowWidgets[i];
        rw.id = row.id;
        rw.kind = row.kind;

        if (row.kind == PropertyRowKind::Header) {
            auto* header = new TextElement();
            header->text = row.label;
            header->color = headerColor;
            header->fontSize = 12.0f;
            header->anchor = { 0.0f, 0.5f };
            header->pivot = { 0.0f, 0.5f };
            header->offset = { 4.0f, 0.0f };
            header->size = { 0.0f, rowHeight };
            root->AddChild(header);
            continue;
        }

        if (row.kind == PropertyRowKind::Button) {
            auto* btn = new ButtonElement();
            btn->size = { 0.0f, rowHeight + 2.0f };
            btn->defaultColor = buttonBg;
            btn->hoveredColor = buttonHover;
            btn->selectedColor = buttonHover;
            btn->pressedColor = buttonHover;
            std::function<void()> callback = row.onButtonPressed;
            btn->onActivated = [callback]() { if (callback) callback(); };
            auto* label = new TextElement();
            label->text = row.label;
            label->fontSize = 11.0f;
            label->anchor = { 0.5f, 0.5f };
            label->pivot = { 0.5f, 0.5f };
            btn->AddChild(label);
            root->AddChild(btn);
            continue;
        }

        if (row.kind == PropertyRowKind::Cells) {
            auto* cellRow = new CellRow();
            cellRow->size = { 0.0f, rowHeight };
            cellRow->indent = row.indent;
            rw.cells.resize(row.cells.size());
            for (size_t c = 0; c < row.cells.size(); c++) {
                const PropertyCell& cell = row.cells[c];
                RowWidgets::CellWidgets& cw = rw.cells[c];
                if (cell.kind == PropertyCellKind::Label) {
                    auto* text = new TextElement();
                    text->fontSize = 11.0f;
                    text->anchor = { 0.0f, 0.5f };
                    text->pivot = { 0.0f, 0.5f };
                    text->offset = { 4.0f, 0.0f };
                    text->size = { cell.width, rowHeight };
                    cellRow->AddChild(text);
                    cw.text = text;
                } else if (cell.kind == PropertyCellKind::Text || cell.kind == PropertyCellKind::Number) {
                    auto* box = new TextElement();
                    box->editable = true;
                    box->numericOnly = (cell.kind == PropertyCellKind::Number);
                    box->fontSize = 11.0f;
                    box->backgroundColor = fieldBg;
                    box->borderColor = fieldBorder;
                    box->editingBorderColor = fieldEditingBorder;
                    box->placeholder = cell.placeholder;
                    box->size = { cell.width, rowHeight };
                    box->onTextCommitted = [this, i, c](const std::string& text) {
                        if (i >= rows.size() || c >= rows[i].cells.size()) return;
                        const PropertyCell& current = rows[i].cells[c];
                        if (current.kind == PropertyCellKind::Number) {
                            if (current.onNumber) current.onNumber((float)atof(text.c_str()));
                        } else if (current.onText) {
                            current.onText(text);
                        }
                    };
                    cellRow->AddChild(box);
                    cw.text = box;
                } else {
                    auto* btn = new ButtonElement();
                    btn->size = { cell.width, rowHeight };
                    btn->defaultColor = cell.color.a > 0 ? cell.color : buttonBg;
                    btn->hoveredColor = buttonHover;
                    btn->selectedColor = buttonHover;
                    btn->pressedColor = buttonHover;
                    btn->onActivated = [this, i, c]() {
                        if (i >= rows.size() || c >= rows[i].cells.size() || i >= rowWidgets.size() || c >= rowWidgets[i].cells.size()) return;
                        const PropertyCell& current = rows[i].cells[c];
                        ButtonElement* self = rowWidgets[i].cells[c].button;
                        if (current.onPressed && self) current.onPressed(self->lastResolvedRect);
                    };
                    auto* label = new TextElement();
                    label->fontSize = 11.0f;
                    label->anchor = { 0.5f, 0.5f };
                    label->pivot = { 0.5f, 0.5f };
                    btn->AddChild(label);
                    cellRow->AddChild(btn);
                    cw.button = btn;
                    cw.buttonLabel = label;
                }
            }
            root->AddChild(cellRow);
            continue;
        }

        if (row.kind == PropertyRowKind::Vec3 || row.kind == PropertyRowKind::Vec2) {
            int count = (row.kind == PropertyRowKind::Vec3) ? 3 : 2;

            auto* label = new TextElement();
            label->text = row.label;
            label->color = labelColor;
            label->fontSize = 10.0f;
            label->anchor = { 0.0f, 0.0f };
            label->pivot = { 0.0f, 0.0f };
            label->offset = { 4.0f, 0.0f };
            label->size = { 0.0f, 14.0f };
            root->AddChild(label);

            auto* triplet = new TripletRow();
            triplet->count = count;
            triplet->size = { 0.0f, rowHeight };
            triplet->hasTrailing = !row.trailingLabel.empty();

            const char* axisNames[3] = { "X", "Y", "Z" };
            Color axisColors[3] = { axisXColor, axisYColor, axisZColor };
            TextElement** boxes[3] = { &rw.vecX, &rw.vecY, &rw.vecZ };

            for (int a = 0; a < count; a++) {
                auto* axisLabel = new TextElement();
                axisLabel->text = axisNames[a];
                axisLabel->color = axisColors[a];
                axisLabel->fontSize = 10.0f;
                axisLabel->anchor = { 0.0f, 0.5f };
                axisLabel->pivot = { 0.0f, 0.5f };
                triplet->AddChild(axisLabel);

                auto* box = new TextElement();
                box->editable = true;
                box->numericOnly = true;
                box->fontSize = 11.0f;
                box->backgroundColor = fieldBg;
                box->borderColor = fieldBorder;
                box->editingBorderColor = fieldEditingBorder;
                box->size = { 0.0f, rowHeight };
                std::function<void(float, float, float)> setVec3 = row.setVec3;
                int axisIndex = a;
                box->onTextChanged = [this, i, setVec3, axisIndex](const std::string& text) {
                    if (i >= rowWidgets.size()) return;
                    float x = rowWidgets[i].vecX ? (float)atof(rowWidgets[i].vecX->text.c_str()) : 0.0f;
                    float y = rowWidgets[i].vecY ? (float)atof(rowWidgets[i].vecY->text.c_str()) : 0.0f;
                    float z = rowWidgets[i].vecZ ? (float)atof(rowWidgets[i].vecZ->text.c_str()) : 0.0f;
                    if (axisIndex == 0) x = (float)atof(text.c_str());
                    if (axisIndex == 1) y = (float)atof(text.c_str());
                    if (axisIndex == 2) z = (float)atof(text.c_str());
                    if (setVec3) setVec3(x, y, z);
                };
                triplet->AddChild(box);
                *boxes[a] = box;
            }
            if (triplet->hasTrailing) addTrailing(triplet, rw, i, row);
            root->AddChild(triplet);
            continue;
        }

        auto* rowBox = new LabeledRow();
        rowBox->size = { 0.0f, rowHeight };
        rowBox->labelWidth = labelWidth;
        rowBox->hasTrailing = !row.trailingLabel.empty();

        auto* label = new TextElement();
        label->text = row.label;
        label->color = labelColor;
        label->fontSize = 11.0f;
        label->anchor = { 0.0f, 0.5f };
        label->pivot = { 0.0f, 0.5f };
        label->offset = { 4.0f, 0.0f };
        rowBox->AddChild(label);

        switch (row.kind) {
            case PropertyRowKind::ReadOnlyText: {
                auto* value = new TextElement();
                value->color = readOnlyColor;
                value->fontSize = 11.0f;
                value->anchor = { 0.0f, 0.5f };
                value->pivot = { 0.0f, 0.5f };
                value->size = { 0.0f, rowHeight };
                rowBox->AddChild(value);
                rw.textField = value;
                break;
            }
            case PropertyRowKind::String:
            case PropertyRowKind::Float: {
                auto* box = new TextElement();
                box->editable = true;
                box->numericOnly = (row.kind == PropertyRowKind::Float);
                box->fontSize = 11.0f;
                box->backgroundColor = fieldBg;
                box->borderColor = fieldBorder;
                box->editingBorderColor = fieldEditingBorder;
                box->size = { 0.0f, rowHeight };
                box->placeholder = row.placeholder;
                std::function<void(const std::string&)> setString = row.setString;
                std::function<void(float)> setFloat = row.setFloat;
                bool isFloat = (row.kind == PropertyRowKind::Float);
                bool isInteger = row.integer;
                if (row.commitOnly && !isFloat) {
                    box->onTextCommitted = [this, i](const std::string& text) {
                        if (i < rows.size() && rows[i].setString) rows[i].setString(text);
                    };
                } else {
                    box->onTextChanged = [setString, setFloat, isFloat, isInteger](const std::string& text) {
                        if (isFloat) {
                            float value = (float)atof(text.c_str());
                            if (isInteger) value = std::round(value);
                            if (setFloat) setFloat(value);
                        } else if (setString) {
                            setString(text);
                        }
                    };
                }
                rowBox->AddChild(box);
                rw.textField = box;
                break;
            }
            case PropertyRowKind::Bool: {
                auto* check = new CheckBoxElement();
                check->size = { rowHeight - 2.0f, rowHeight - 2.0f };
                check->boxColor = checkBoxColor;
                check->hoveredBoxColor = checkBoxHoveredColor;
                check->borderColor = checkBoxBorderColor;
                check->checkColor = checkBoxCheckColor;
                std::function<void(bool)> setBool = row.setBool;
                check->onToggled = [setBool](bool value) { if (setBool) setBool(value); };
                rowBox->AddChild(check);
                rw.checkBox = check;
                break;
            }
            case PropertyRowKind::Asset: {
                auto* btn = new ButtonElement();
                btn->size = { 0.0f, rowHeight };
                btn->defaultColor = fieldBg;
                btn->hoveredColor = buttonHover;
                btn->selectedColor = buttonHover;
                btn->pressedColor = buttonHover;
                std::string rowId = row.id;
                std::string extension = row.assetExtension;
                btn->onActivated = [this, rowId, extension]() {
                    if (onAssetFieldClicked) onAssetFieldClicked(rowId, extension, GetMousePosition());
                };
                auto* value = new TextElement();
                value->fontSize = 11.0f;
                value->anchor = { 0.0f, 0.5f };
                value->pivot = { 0.0f, 0.5f };
                value->offset = { 4.0f, 0.0f };
                value->size = { 0.0f, rowHeight - 2.0f };
                btn->AddChild(value);
                rowBox->AddChild(btn);
                rw.assetOrEnumButton = btn;
                rw.assetOrEnumLabel = value;
                break;
            }
            case PropertyRowKind::Enum: {
                auto* btn = new ButtonElement();
                btn->size = { 0.0f, rowHeight };
                btn->defaultColor = fieldBg;
                btn->hoveredColor = buttonHover;
                btn->selectedColor = buttonHover;
                btn->pressedColor = buttonHover;
                std::function<void(const std::string&)> setString = row.setString;
                std::vector<std::string> options = row.enumOptions;
                btn->onActivated = [this, i, setString, options]() {
                    if (options.empty() || !setString) return;
                    if (i >= rows.size()) return;
                    const std::string& current = rows[i].stringValue;
                    size_t idx = 0;
                    for (size_t o = 0; o < options.size(); o++) {
                        if (options[o] == current) { idx = o; break; }
                    }
                    idx = (idx + 1) % options.size();
                    setString(options[idx]);
                };
                auto* value = new TextElement();
                value->fontSize = 11.0f;
                value->anchor = { 0.0f, 0.5f };
                value->pivot = { 0.0f, 0.5f };
                value->offset = { 4.0f, 0.0f };
                value->size = { 0.0f, rowHeight - 2.0f };
                btn->AddChild(value);
                rowBox->AddChild(btn);
                rw.assetOrEnumButton = btn;
                rw.assetOrEnumLabel = value;
                break;
            }
            default: break;
        }

        if (rowBox->hasTrailing) addTrailing(rowBox, rw, i, row);
        root->AddChild(rowBox);
    }

    RefreshValues();
}

void PropertyPanel::RefreshValues() {
    for (size_t i = 0; i < rows.size() && i < rowWidgets.size(); i++) {
        const PropertyRow& row = rows[i];
        RowWidgets& rw = rowWidgets[i];

        switch (row.kind) {
            case PropertyRowKind::ReadOnlyText:
                if (rw.textField) rw.textField->text = row.stringValue;
                break;
            case PropertyRowKind::String:
                if (rw.textField && !rw.textField->isEditing) rw.textField->text = row.stringValue;
                break;
            case PropertyRowKind::Float:
                if (rw.textField && !rw.textField->isEditing) rw.textField->text = row.integer ? FormatInteger(row.floatValue) : FormatFloat(row.floatValue);
                break;
            case PropertyRowKind::Bool:
                if (rw.checkBox) rw.checkBox->checked = row.boolValue;
                break;
            case PropertyRowKind::Vec3:
                if (rw.vecX && !rw.vecX->isEditing) rw.vecX->text = FormatFloat(row.vec3[0]);
                if (rw.vecY && !rw.vecY->isEditing) rw.vecY->text = FormatFloat(row.vec3[1]);
                if (rw.vecZ && !rw.vecZ->isEditing) rw.vecZ->text = FormatFloat(row.vec3[2]);
                break;
            case PropertyRowKind::Vec2:
                if (rw.vecX && !rw.vecX->isEditing) rw.vecX->text = FormatFloat(row.vec3[0]);
                if (rw.vecY && !rw.vecY->isEditing) rw.vecY->text = FormatFloat(row.vec3[1]);
                break;
            case PropertyRowKind::Asset:
            case PropertyRowKind::Enum:
                if (rw.assetOrEnumLabel) rw.assetOrEnumLabel->text = row.stringValue;
                if (rw.assetOrEnumButton && row.iconColor.a > 0) rw.assetOrEnumButton->defaultColor = row.iconColor;
                break;
            case PropertyRowKind::Cells:
                for (size_t c = 0; c < row.cells.size() && c < rw.cells.size(); c++) {
                    const PropertyCell& cell = row.cells[c];
                    RowWidgets::CellWidgets& cw = rw.cells[c];
                    if (cw.text) {
                        if (cell.kind == PropertyCellKind::Label) {
                            cw.text->text = cell.text;
                            cw.text->color = cell.textColor.a > 0 ? cell.textColor : labelColor;
                        } else if (!cw.text->isEditing) {
                            cw.text->text = (cell.kind == PropertyCellKind::Number)
                                ? (cell.integer ? FormatInteger(cell.number) : FormatFloat(cell.number))
                                : cell.text;
                        }
                    }
                    if (cw.button) {
                        cw.button->defaultColor = cell.color.a > 0 ? cell.color : buttonBg;
                        if (cw.buttonLabel) {
                            cw.buttonLabel->text = cell.text;
                            cw.buttonLabel->color = cell.textColor.a > 0 ? cell.textColor : Color{ 255, 255, 255, 255 };
                        }
                    }
                }
                break;
            default:
                break;
        }

        if (rw.trailingButton) {
            rw.trailingButton->defaultColor = row.trailingColor.a > 0 ? row.trailingColor : buttonBg;
            if (rw.trailingLabel) rw.trailingLabel->text = row.trailingLabel;
        }
    }
}
