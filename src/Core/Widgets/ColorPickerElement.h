#pragma once
#include "WidgetElement.h"
#include <functional>

class SliderElement;
class RectangleElement;

class ColorPickerElement : public WidgetElement {
public:
    ColorPickerElement();

    const char* GetClassName() const override { return "ColorPickerElement"; }
    std::string GetObjectKind() const override { return "ColorPickerElement"; }

    Color color{ 255, 255, 255, 255 };
    bool showAlpha = true;

    std::function<void(Color)> onColorChanged;

    void SetColor(Color newColor);

    const std::vector<WidgetElement*>& GetAuthoredChildren() const override;
    void ClearChildren() override;

protected:
    void OnTickSelf(float deltaTime, Rectangle rect, float uiScale) override;
    Rectangle ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const override;

private:
    void BuildParts();
    void SyncParts();
    void OnSliderChanged();

    RectangleElement* swatch = nullptr;
    SliderElement* sliders[4] = { nullptr, nullptr, nullptr, nullptr };
    Color lastSynced{ 0, 0, 0, 0 };
    bool synced = false;
};
