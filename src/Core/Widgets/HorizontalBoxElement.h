#pragma once
#include "BoxElement.h"

class HorizontalBoxElement : public BoxElement {
public:
    HorizontalBoxElement() { axis = BoxAxis::Horizontal; }

    const char* GetClassName() const override { return "HorizontalBoxElement"; }
    std::string GetObjectKind() const override { return "HorizontalBoxElement"; }
};
