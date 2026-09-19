#include "WidgetElementDetailsBuilder.h"
#include "Core/Widgets/WidgetElement.h"
#include "Core/Widgets/WidgetElementFields.h"
#include "Core/SafeParse.h"
#include <cstdio>

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

    std::string className = element.GetClassName();
    for (const WidgetElementFieldDesc& desc : GetWidgetElementFields()) {
        if (className != desc.elementType) continue;

        PropertyRow row;
        row.id = std::string("field:") + desc.key;
        row.label = desc.key;

        if (desc.kind == WidgetFieldKind::Bool) {
            row.kind = PropertyRowKind::Bool;
            row.boolValue = desc.getValue(element) == "true";
            row.setBool = [currentElement, onEdited, desc](bool v) {
                WidgetElement* e = currentElement();
                if (!e) return;
                desc.setValue(*e, v ? "true" : "false");
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
                desc.setValue(*e, v);
                if (onEdited) onEdited();
            };
        }

        rows.push_back(row);
    }

    return rows;
}
