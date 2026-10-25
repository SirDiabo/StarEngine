#pragma once
#include "Editor/PropertyPanel.h"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace PropertyRows {

inline std::string StemOrNone(const std::string& path) {
    return path.empty() ? std::string("None") : std::filesystem::path(path).stem().string();
}

inline PropertyRow HeaderRow(const std::string& id, const std::string& text) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Header;
    row.label = text;
    return row;
}

inline PropertyRow ReadOnlyRow(const std::string& id, const std::string& label, const std::string& value) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::ReadOnlyText;
    row.label = label;
    row.stringValue = value;
    return row;
}

inline PropertyRow StringRow(const std::string& id, const std::string& label, const std::string& value,
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

inline PropertyRow EnumRow(const std::string& id, const std::string& label, const std::string& value,
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

inline PropertyRow BoolRow(const std::string& id, const std::string& label, bool value, std::function<void(bool)> setter) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Bool;
    row.label = label;
    row.boolValue = value;
    row.setBool = std::move(setter);
    return row;
}

inline PropertyRow FloatRow(const std::string& id, const std::string& label, float value, bool integer,
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

inline PropertyRow Vec3Row(const std::string& id, const std::string& label, float x, float y, float z,
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

inline PropertyRow AssetRow(const std::string& id, const std::string& label, const std::string& extension, const std::string& display) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Asset;
    row.label = label;
    row.assetExtension = extension;
    row.stringValue = display;
    return row;
}

inline PropertyRow ButtonRow(const std::string& id, const std::string& label, std::function<void()> onPressed) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Button;
    row.label = label;
    row.onButtonPressed = std::move(onPressed);
    return row;
}

inline PropertyRow CellsRow(const std::string& id, std::vector<PropertyCell> cells, float indent = 0.0f) {
    PropertyRow row;
    row.id = id;
    row.kind = PropertyRowKind::Cells;
    row.cells = std::move(cells);
    row.indent = indent;
    return row;
}

inline PropertyCell LabelCell(const std::string& text, float width) {
    PropertyCell cell;
    cell.kind = PropertyCellKind::Label;
    cell.text = text;
    cell.width = width;
    return cell;
}

inline PropertyCell ButtonCell(const std::string& text, float width, std::function<void(Rectangle)> onPressed, Color color = { 0, 0, 0, 0 }) {
    PropertyCell cell;
    cell.kind = PropertyCellKind::Button;
    cell.text = text;
    cell.width = width;
    cell.color = color;
    cell.onPressed = std::move(onPressed);
    return cell;
}

inline PropertyCell NumberCell(float value, bool integer, float width, std::function<void(float)> onNumber) {
    PropertyCell cell;
    cell.kind = PropertyCellKind::Number;
    cell.number = value;
    cell.integer = integer;
    cell.width = width;
    cell.onNumber = std::move(onNumber);
    return cell;
}

inline PropertyCell TextCell(const std::string& text, float width, const std::string& placeholder, std::function<void(const std::string&)> onText) {
    PropertyCell cell;
    cell.kind = PropertyCellKind::Text;
    cell.text = text;
    cell.width = width;
    cell.placeholder = placeholder;
    cell.onText = std::move(onText);
    return cell;
}

}
