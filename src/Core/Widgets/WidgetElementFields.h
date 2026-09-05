#pragma once
#include "WidgetElement.h"
#include <functional>
#include <string>
#include <vector>

enum class WidgetFieldKind {
    String,
    Float,
    Int,
    Bool,
    Color
};

struct WidgetElementFieldDesc {
    const char* elementType = nullptr;
    const char* key = nullptr;
    WidgetFieldKind kind = WidgetFieldKind::String;
    std::function<std::string(const WidgetElement&)> getValue;
    std::function<void(WidgetElement&, const std::string&)> setValue;
};

using WidgetElementFactory = std::function<WidgetElement*()>;

struct WidgetElementTypeDesc {
    const char* elementType = nullptr;
    WidgetElementFactory factory;
};

const std::vector<WidgetElementFieldDesc>& GetWidgetElementFields();
const std::vector<WidgetElementTypeDesc>& GetWidgetElementTypes();
WidgetElement* CreateDefaultWidgetElement(const std::string& elementType);
