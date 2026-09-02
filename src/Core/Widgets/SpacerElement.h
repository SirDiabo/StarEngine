#pragma once
#include "WidgetElement.h"

class SpacerElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "SpacerElement"; }
    std::string GetObjectKind() const override { return "SpacerElement"; }
};
