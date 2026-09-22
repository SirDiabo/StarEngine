#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

class WidgetElement;
class VerticalBoxElement;
class TextElement;
class ButtonElement;
class CheckBoxElement;

enum class PropertyRowKind {
    Header,
    ReadOnlyText,
    String,
    Float,
    Bool,
    Vec2,
    Vec3,
    Asset,
    Enum,
    Button,
    Cells
};

enum class PropertyCellKind {
    Label,
    Text,
    Number,
    Button
};

struct PropertyCell {
    PropertyCellKind kind = PropertyCellKind::Button;
    float width = 0.0f;
    std::string text;
    float number = 0.0f;
    bool integer = false;
    Color color{ 0, 0, 0, 0 };
    Color textColor{ 0, 0, 0, 0 };
    std::string placeholder;
    std::function<void(const std::string&)> onText;
    std::function<void(float)> onNumber;
    std::function<void(Rectangle)> onPressed;
};

struct PropertyRow {
    std::string id;
    PropertyRowKind kind = PropertyRowKind::String;
    std::string label;

    std::string stringValue;
    float floatValue = 0.0f;
    bool boolValue = false;
    float vec3[3] = { 0.0f, 0.0f, 0.0f };
    std::vector<std::string> enumOptions;
    std::string assetExtension;
    Color iconColor{ 0, 0, 0, 0 };

    bool integer = false;
    bool commitOnly = false;
    std::string placeholder;
    std::vector<PropertyCell> cells;
    float indent = 0.0f;
    std::string trailingLabel;
    Color trailingColor{ 0, 0, 0, 0 };
    std::function<void(Rectangle)> onTrailingPressed;

    std::function<void(const std::string&)> setString;
    std::function<void(float)> setFloat;
    std::function<void(bool)> setBool;
    std::function<void(float, float, float)> setVec3;
    std::function<void()> onButtonPressed;
};

class PropertyPanel {
public:
    PropertyPanel();

    WidgetElement* GetRootElement() const;

    Color labelColor{ 190, 190, 195, 255 };
    Color headerColor{ 220, 220, 225, 255 };
    Color readOnlyColor{ 140, 140, 145, 255 };
    Color fieldBg{ 35, 35, 40, 255 };
    Color fieldBorder{ 70, 70, 75, 255 };
    Color fieldEditingBorder{ 90, 140, 220, 255 };
    Color buttonBg{ 60, 60, 65, 255 };
    Color buttonHover{ 80, 80, 85, 255 };
    Color axisXColor{ 225, 100, 100, 255 };
    Color axisYColor{ 110, 210, 120, 255 };
    Color axisZColor{ 100, 150, 230, 255 };
    Color checkBoxColor{ 40, 40, 45, 255 };
    Color checkBoxHoveredColor{ 55, 55, 60, 255 };
    Color checkBoxBorderColor{ 90, 90, 95, 255 };
    Color checkBoxCheckColor{ 90, 140, 220, 255 };
    float rowHeight = 20.0f;
    float labelWidth = 90.0f;

    std::function<void(const std::string& rowId, const std::string& extension, Vector2 screenPos)> onAssetFieldClicked;

    void SetProperties(std::vector<PropertyRow> rows);

private:
    void RebuildRows();
    void RefreshValues();

    VerticalBoxElement* root = nullptr;
    std::vector<PropertyRow> rows;

    struct RowWidgets {
        std::string id;
        PropertyRowKind kind = PropertyRowKind::String;
        TextElement* textField = nullptr;
        CheckBoxElement* checkBox = nullptr;
        TextElement* vecX = nullptr;
        TextElement* vecY = nullptr;
        TextElement* vecZ = nullptr;
        ButtonElement* assetOrEnumButton = nullptr;
        TextElement* assetOrEnumLabel = nullptr;
        ButtonElement* trailingButton = nullptr;
        TextElement* trailingLabel = nullptr;

        struct CellWidgets {
            TextElement* text = nullptr;
            ButtonElement* button = nullptr;
            TextElement* buttonLabel = nullptr;
        };
        std::vector<CellWidgets> cells;
    };
    std::vector<RowWidgets> rowWidgets;
};
