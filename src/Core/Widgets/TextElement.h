#pragma once
#include "WidgetElement.h"
#include <string>

class TextElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "TextElement"; }
    std::string GetObjectKind() const override { return "TextElement"; }

    std::string text = "Text";
    Color color{ 255, 255, 255, 255 };
    float fontSize = 16.0f;

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
};
