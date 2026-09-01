#pragma once
#include "BoxElement.h"

class VerticalBoxElement : public BoxElement {
public:
    VerticalBoxElement() { axis = BoxAxis::Vertical; }

    const char* GetClassName() const override { return "VerticalBoxElement"; }
    std::string GetObjectKind() const override { return "VerticalBoxElement"; }
};
