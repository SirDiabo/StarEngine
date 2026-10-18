#include "WidgetElementDetailsBuilder.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/WidgetElementFields.h"
#include "Core/Widgets/WidgetObjectElement.h"
#include "Core/Widgets/SlotElement.h"
#include "Core/SafeParse.h"
#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <cstdlib>

namespace {

Color ParseColorField(const std::string& text) {
    int r = 255, g = 255, b = 255, a = 255;
    sscanf(text.c_str(), "%d,%d,%d,%d", &r, &g, &b, &a);
    return Color{ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a };
}

}

namespace fs = std::filesystem;

std::vector<PropertyRow> BuildWidgetElementPropertyRows(
    WidgetElement& element,
    const std::function<WidgetElement*()>& currentElement,
    const std::function<void()>& onEdited) {

    std::vector<PropertyRow> rows;

    PropertyRow header;
    header.kind = PropertyRowKind::Header;
    header.label = element.GetClassName();
    rows.push_back(header);

    PropertyRow idRow;
    idRow.id = "elementId";
    idRow.kind = PropertyRowKind::String;
    idRow.label = "Element Id";
    idRow.stringValue = element.elementId;
    idRow.setString = [currentElement, onEdited](const std::string& v) {
        WidgetElement* e = currentElement();
        if (!e) return;
        e->elementId = v;
        if (onEdited) onEdited();
    };
    rows.push_back(idRow);

    PropertyRow visibleRow;
    visibleRow.id = "visible";
    visibleRow.kind = PropertyRowKind::Bool;
    visibleRow.label = "Visible";
    visibleRow.boolValue = element.visible;
    visibleRow.setBool = [currentElement, onEdited](bool v) {
        WidgetElement* e = currentElement();
        if (!e) return;
        e->visible = v;
        if (onEdited) onEdited();
    };
    rows.push_back(visibleRow);

    PropertyRow selectableRow;
    selectableRow.id = "selectable";
    selectableRow.kind = PropertyRowKind::Bool;
    selectableRow.label = "Selectable (runtime)";
    selectableRow.boolValue = element.selectable;
    selectableRow.setBool = [currentElement, onEdited](bool v) {
        WidgetElement* e = currentElement();
        if (!e) return;
        e->selectable = v;
        if (onEdited) onEdited();
    };
    rows.push_back(selectableRow);

    auto vec2Row = [&](const char* id, const char* label, Vector2 value,
                        std::function<void(WidgetElement&, Vector2)> setter) {
        PropertyRow row;
        row.id = id;
        row.kind = PropertyRowKind::Vec2;
        row.label = label;
        row.vec3[0] = value.x;
        row.vec3[1] = value.y;
        row.setVec3 = [currentElement, onEdited, setter](float x, float y, float) {
            WidgetElement* e = currentElement();
            if (!e) return;
            setter(*e, { x, y });
            if (onEdited) onEdited();
        };
        rows.push_back(row);
    };

    vec2Row("anchor", "Anchor", element.anchor, [](WidgetElement& e, Vector2 v) { e.anchor = v; });
    vec2Row("pivot", "Pivot", element.pivot, [](WidgetElement& e, Vector2 v) { e.pivot = v; });
    vec2Row("offset", "Offset", element.offset, [](WidgetElement& e, Vector2 v) { e.offset = v; });
    vec2Row("size", "Size", element.size, [](WidgetElement& e, Vector2 v) { e.size = v; });

    PropertyRow marginRow;
    marginRow.id = "margin";
    marginRow.kind = PropertyRowKind::Vec4;
    marginRow.label = "Margin";
    marginRow.axisLabels = { "L", "T", "R", "B" };
    marginRow.vec4[0] = element.margin.left;
    marginRow.vec4[1] = element.margin.top;
    marginRow.vec4[2] = element.margin.right;
    marginRow.vec4[3] = element.margin.bottom;
    marginRow.setVec4 = [currentElement, onEdited](float l, float t, float r, float b) {
        WidgetElement* e = currentElement();
        if (!e) return;
        e->margin = WidgetMargin{ l, t, r, b };
        if (onEdited) onEdited();
    };
    rows.push_back(marginRow);

    vec2Row("minSize", "Min Size (0 = none)", element.minSize, [](WidgetElement& e, Vector2 v) { e.minSize = { std::max(0.0f, v.x), std::max(0.0f, v.y) }; });
    vec2Row("maxSize", "Max Size (0 = none)", element.maxSize, [](WidgetElement& e, Vector2 v) { e.maxSize = { std::max(0.0f, v.x), std::max(0.0f, v.y) }; });

    WidgetObjectElement* fillOwner = nullptr;
    if (WidgetElement* host = element.parent) {
        fillOwner = dynamic_cast<WidgetObjectElement*>(host);
        if (!fillOwner && dynamic_cast<SlotElement*>(host)) {
            for (WidgetElement* e = host->parent; e && !fillOwner; e = e->parent) fillOwner = dynamic_cast<WidgetObjectElement*>(e);
        }
    }
    if (fillOwner) {
        PropertyRow slotRow;
        slotRow.id = "slotName";
        slotRow.label = "Slot";
        slotRow.kind = PropertyRowKind::String;
        slotRow.stringValue = element.slotName;
        slotRow.setString = [currentElement, onEdited, fillOwner](const std::string& v) {
            WidgetElement* e = currentElement();
            if (!e) return;
            fillOwner->SetFillSlot(e, v);
            if (onEdited) onEdited();
        };
        rows.push_back(slotRow);
    }

    std::string className = element.GetClassName();
    for (const WidgetElementFieldDesc& desc : GetWidgetElementFields()) {
        if (className != desc.elementType) continue;

        PropertyRow row;
        row.id = std::string("field:") + desc.key;
        row.label = desc.key;

        if (desc.elementType == "WidgetObjectElement" && desc.key == "WidgetAsset") {
            std::string value = desc.getValue(element);
            row.id = "widgetAsset";
            row.label = "Widget";
            row.kind = PropertyRowKind::Asset;
            row.assetExtension = ".starwidget";
            row.stringValue = value.empty() ? "None" : fs::path(value).stem().string();
            rows.push_back(row);
            continue;
        }

        if (desc.kind == WidgetFieldKind::Bool) {
            row.kind = PropertyRowKind::Bool;
            row.boolValue = desc.getValue(element) == "true";
            row.setBool = [currentElement, onEdited, desc](bool v) {
                WidgetElement* e = currentElement();
                if (!e) return;
                desc.setValue(*e, v ? "true" : "false");
                if (onEdited) onEdited();
            };
        } else if (desc.kind == WidgetFieldKind::Color) {
            row.kind = PropertyRowKind::Color;
            row.colorValue = ParseColorField(desc.getValue(element));
            row.setColor = [currentElement, onEdited, desc](Color c) {
                WidgetElement* e = currentElement();
                if (!e) return;
                char buf[32];
                snprintf(buf, sizeof(buf), "%d,%d,%d,%d", (int)c.r, (int)c.g, (int)c.b, (int)c.a);
                desc.setValue(*e, buf);
                if (onEdited) onEdited();
            };
        } else if (desc.kind == WidgetFieldKind::Float || desc.kind == WidgetFieldKind::Int) {
            row.kind = PropertyRowKind::Float;
            row.floatValue = SafeParse::ToFloat(desc.getValue(element));
            row.setFloat = [currentElement, onEdited, desc](float v) {
                WidgetElement* e = currentElement();
                if (!e) return;
                char buf[32];
                snprintf(buf, sizeof(buf), "%g", v);
                desc.setValue(*e, buf);
                if (onEdited) onEdited();
            };
        } else {
            row.kind = PropertyRowKind::String;
            row.stringValue = desc.getValue(element);
            row.setString = [currentElement, onEdited, desc](const std::string& v) {
                WidgetElement* e = currentElement();
                if (!e) return;
                WidgetObjectElement::BehaviorScope inert(false);
                desc.setValue(*e, v);
                if (onEdited) onEdited();
            };
        }

        rows.push_back(row);
    }

    return rows;
}
