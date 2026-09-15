#include "WidgetElementFields.h"
#include "TextElement.h"
#include "RectangleElement.h"
#include "ButtonElement.h"
#include "VerticalBoxElement.h"
#include "HorizontalBoxElement.h"
#include "GridElement.h"
#include "ImageElement.h"
#include "ProgressBarElement.h"
#include "SpacerElement.h"
#include "SplitBoxElement.h"
#include "TabContainer.h"
#include "SearchListElement.h"
#include "Core/SafeParse.h"
#include <cstdio>

namespace {

std::string ColorToString(Color c) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d,%d,%d,%d", (int)c.r, (int)c.g, (int)c.b, (int)c.a);
    return buf;
}

Color ColorFromString(const std::string& s) {
    int r = 255, g = 255, b = 255, a = 255;
    sscanf(s.c_str(), "%d,%d,%d,%d", &r, &g, &b, &a);
    return Color{ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a };
}

template<typename T>
WidgetElementFieldDesc StrF(const char* type, const char* key, std::string T::* field) {
    WidgetElementFieldDesc d;
    d.elementType = type;
    d.key = key;
    d.kind = WidgetFieldKind::String;
    d.getValue = [field](const WidgetElement& e) { return static_cast<const T&>(e).*field; };
    d.setValue = [field](WidgetElement& e, const std::string& v) { static_cast<T&>(e).*field = v; };
    return d;
}

template<typename T>
WidgetElementFieldDesc FltF(const char* type, const char* key, float T::* field) {
    WidgetElementFieldDesc d;
    d.elementType = type;
    d.key = key;
    d.kind = WidgetFieldKind::Float;
    d.getValue = [field](const WidgetElement& e) { return std::to_string(static_cast<const T&>(e).*field); };
    d.setValue = [field](WidgetElement& e, const std::string& v) { static_cast<T&>(e).*field = SafeParse::ToFloat(v); };
    return d;
}

template<typename T>
WidgetElementFieldDesc IntF(const char* type, const char* key, int T::* field) {
    WidgetElementFieldDesc d;
    d.elementType = type;
    d.key = key;
    d.kind = WidgetFieldKind::Int;
    d.getValue = [field](const WidgetElement& e) { return std::to_string(static_cast<const T&>(e).*field); };
    d.setValue = [field](WidgetElement& e, const std::string& v) { static_cast<T&>(e).*field = SafeParse::ToInt(v); };
    return d;
}

template<typename T>
WidgetElementFieldDesc BlnF(const char* type, const char* key, bool T::* field) {
    WidgetElementFieldDesc d;
    d.elementType = type;
    d.key = key;
    d.kind = WidgetFieldKind::Bool;
    d.getValue = [field](const WidgetElement& e) { return (static_cast<const T&>(e).*field) ? std::string("true") : std::string("false"); };
    d.setValue = [field](WidgetElement& e, const std::string& v) { static_cast<T&>(e).*field = (v == "true"); };
    return d;
}

template<typename T>
WidgetElementFieldDesc ColF(const char* type, const char* key, Color T::* field) {
    WidgetElementFieldDesc d;
    d.elementType = type;
    d.key = key;
    d.kind = WidgetFieldKind::Color;
    d.getValue = [field](const WidgetElement& e) { return ColorToString(static_cast<const T&>(e).*field); };
    d.setValue = [field](WidgetElement& e, const std::string& v) { static_cast<T&>(e).*field = ColorFromString(v); };
    return d;
}

WidgetElementFieldDesc SplitAxisF(const char* type, const char* key, BoxAxis SplitBoxElement::* field) {
    WidgetElementFieldDesc d;
    d.elementType = type;
    d.key = key;
    d.kind = WidgetFieldKind::Int;
    d.getValue = [field](const WidgetElement& e) { return std::to_string((int)(static_cast<const SplitBoxElement&>(e).*field)); };
    d.setValue = [field](WidgetElement& e, const std::string& v) { static_cast<SplitBoxElement&>(e).*field = (BoxAxis)SafeParse::ToInt(v); };
    return d;
}

}

const std::vector<WidgetElementFieldDesc>& GetWidgetElementFields() {
    static const std::vector<WidgetElementFieldDesc> fields = {
        StrF<TextElement>("TextElement", "Text", &TextElement::text),
        FltF<TextElement>("TextElement", "FontSize", &TextElement::fontSize),
        ColF<TextElement>("TextElement", "Color", &TextElement::color),
        BlnF<TextElement>("TextElement", "Editable", &TextElement::editable),
        BlnF<TextElement>("TextElement", "NumericOnly", &TextElement::numericOnly),
        ColF<TextElement>("TextElement", "BackgroundColor", &TextElement::backgroundColor),
        ColF<TextElement>("TextElement", "BorderColor", &TextElement::borderColor),
        StrF<TextElement>("TextElement", "Placeholder", &TextElement::placeholder),
        IntF<TextElement>("TextElement", "MaxLength", &TextElement::maxLength),

        ColF<RectangleElement>("RectangleElement", "Color", &RectangleElement::color),

        ColF<ButtonElement>("ButtonElement", "DefaultColor", &ButtonElement::defaultColor),
        ColF<ButtonElement>("ButtonElement", "HoveredColor", &ButtonElement::hoveredColor),
        ColF<ButtonElement>("ButtonElement", "SelectedColor", &ButtonElement::selectedColor),
        ColF<ButtonElement>("ButtonElement", "PressedColor", &ButtonElement::pressedColor),
        ColF<ButtonElement>("ButtonElement", "DisabledColor", &ButtonElement::disabledColor),

        BlnF<BoxElement>("VerticalBoxElement", "ScrollEnabled", &BoxElement::scrollEnabled),
        FltF<BoxElement>("VerticalBoxElement", "ScrollOffset", &BoxElement::scrollOffset),
        FltF<BoxElement>("VerticalBoxElement", "ChildSpacing", &BoxElement::childSpacing),
        BlnF<BoxElement>("VerticalBoxElement", "WrapNavigation", &BoxElement::wrapNavigation),
        ColF<BoxElement>("VerticalBoxElement", "BackgroundColor", &BoxElement::backgroundColor),

        BlnF<BoxElement>("HorizontalBoxElement", "ScrollEnabled", &BoxElement::scrollEnabled),
        FltF<BoxElement>("HorizontalBoxElement", "ScrollOffset", &BoxElement::scrollOffset),
        FltF<BoxElement>("HorizontalBoxElement", "ChildSpacing", &BoxElement::childSpacing),
        BlnF<BoxElement>("HorizontalBoxElement", "WrapNavigation", &BoxElement::wrapNavigation),
        ColF<BoxElement>("HorizontalBoxElement", "BackgroundColor", &BoxElement::backgroundColor),

        IntF<GridElement>("GridElement", "Columns", &GridElement::columns),
        FltF<GridElement>("GridElement", "ColumnWidth", &GridElement::columnWidth),
        FltF<GridElement>("GridElement", "RowHeight", &GridElement::rowHeight),
        FltF<GridElement>("GridElement", "CellSpacing", &GridElement::cellSpacing),

        StrF<ImageElement>("ImageElement", "TexturePath", &ImageElement::texturePath),
        ColF<ImageElement>("ImageElement", "Tint", &ImageElement::tint),

        FltF<ProgressBarElement>("ProgressBarElement", "Value", &ProgressBarElement::value),
        ColF<ProgressBarElement>("ProgressBarElement", "BackgroundColor", &ProgressBarElement::backgroundColor),
        ColF<ProgressBarElement>("ProgressBarElement", "FillColor", &ProgressBarElement::fillColor),

        FltF<SplitBoxElement>("SplitBoxElement", "SplitRatio", &SplitBoxElement::splitRatio),
        FltF<SplitBoxElement>("SplitBoxElement", "DividerThickness", &SplitBoxElement::dividerThickness),
        ColF<SplitBoxElement>("SplitBoxElement", "DividerColor", &SplitBoxElement::dividerColor),
        SplitAxisF("SplitBoxElement", "Axis", &SplitBoxElement::axis),

        FltF<SearchListElement>("SearchListElement", "SearchBarHeight", &SearchListElement::searchBarHeight),
        FltF<SearchListElement>("SearchListElement", "RowHeight", &SearchListElement::rowHeight),
        FltF<SearchListElement>("SearchListElement", "CategoryHeaderHeight", &SearchListElement::categoryHeaderHeight),
        ColF<SearchListElement>("SearchListElement", "BackgroundColor", &SearchListElement::backgroundColor),
        ColF<SearchListElement>("SearchListElement", "CategoryHeaderColor", &SearchListElement::categoryHeaderColor),
        ColF<SearchListElement>("SearchListElement", "RowColor", &SearchListElement::rowColor),
        ColF<SearchListElement>("SearchListElement", "RowHoveredColor", &SearchListElement::rowHoveredColor),
    };
    return fields;
}

const std::vector<WidgetElementTypeDesc>& GetWidgetElementTypes() {
    static const std::vector<WidgetElementTypeDesc> types = {
        { "TextElement", []() -> WidgetElement* { return new TextElement(); } },
        { "RectangleElement", []() -> WidgetElement* { return new RectangleElement(); } },
        { "ButtonElement", []() -> WidgetElement* { return new ButtonElement(); } },
        { "VerticalBoxElement", []() -> WidgetElement* { return new VerticalBoxElement(); } },
        { "HorizontalBoxElement", []() -> WidgetElement* { return new HorizontalBoxElement(); } },
        { "GridElement", []() -> WidgetElement* { return new GridElement(); } },
        { "ImageElement", []() -> WidgetElement* { return new ImageElement(); } },
        { "ProgressBarElement", []() -> WidgetElement* { return new ProgressBarElement(); } },
        { "SpacerElement", []() -> WidgetElement* { return new SpacerElement(); } },
        { "SplitBoxElement", []() -> WidgetElement* { return new SplitBoxElement(); } },
        { "TabContainer", []() -> WidgetElement* { return new TabContainer(); } },
        { "TabHeaderButtonElement", []() -> WidgetElement* { return new TabHeaderButtonElement(); } },
        { "SearchListElement", []() -> WidgetElement* { return new SearchListElement(); } },
    };
    return types;
}

WidgetElement* CreateDefaultWidgetElement(const std::string& elementType) {
    for (const WidgetElementTypeDesc& t : GetWidgetElementTypes()) {
        if (elementType == t.elementType) return t.factory();
    }
    return nullptr;
}
