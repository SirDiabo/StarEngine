#pragma once
#include "PropertyPanel.h"
#include <functional>
#include <vector>

class WidgetElement;

std::vector<PropertyRow> BuildWidgetElementPropertyRows(
    WidgetElement& element,
    const std::function<WidgetElement*()>& currentElement,
    const std::function<void()>& onEdited);
