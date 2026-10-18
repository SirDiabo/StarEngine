#include "ColorPickerElement.h"
#include "SliderElement.h"
#include "RectangleElement.h"
#include "Widget.h"
#include <algorithm>
#include <cmath>

namespace {
const float kSwatchHeight = 24.0f;
const float kRowHeight = 18.0f;
const float kGap = 4.0f;

const std::vector<WidgetElement*>& NoChildren() {
    static const std::vector<WidgetElement*> none;
    return none;
}

unsigned char ToByte(float v) {
    return (unsigned char)std::clamp((int)std::lround(v), 0, 255);
}

bool SameColor(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
}

ColorPickerElement::ColorPickerElement() {
    size = { 160.0f, kSwatchHeight + kGap + 4.0f * kRowHeight + 3.0f * kGap };
    BuildParts();
}

void ColorPickerElement::BuildParts() {
    swatch = new RectangleElement();
    swatch->borderThickness = 1.0f;
    swatch->borderColor = Color{ 80, 80, 85, 255 };
    WidgetElement::AddChild(swatch);

    for (int i = 0; i < 4; i++) {
        SliderElement* slider = new SliderElement();
        slider->minValue = 0.0f;
        slider->maxValue = 255.0f;
        slider->onValueChanged = [this](float) { OnSliderChanged(); };
        WidgetElement::AddChild(slider);
        sliders[i] = slider;
    }
    synced = false;
}

void ColorPickerElement::ClearChildren() {
    WidgetElement::ClearChildren();
    swatch = nullptr;
    for (SliderElement*& slider : sliders) slider = nullptr;
    BuildParts();
}

const std::vector<WidgetElement*>& ColorPickerElement::GetAuthoredChildren() const {
    return NoChildren();
}

void ColorPickerElement::SetColor(Color newColor) {
    color = newColor;
    SyncParts();
}

void ColorPickerElement::OnSliderChanged() {
    color = Color{ ToByte(sliders[0]->value), ToByte(sliders[1]->value), ToByte(sliders[2]->value),
                   showAlpha ? ToByte(sliders[3]->value) : color.a };
    SyncParts();
    if (onColorChanged) onColorChanged(color);
    if (owningWidget) owningWidget->FireElementEvent(this, "ColorChanged");
}

void ColorPickerElement::SyncParts() {
    if (!swatch) return;
    swatch->color = color;
    sliders[0]->value = color.r;
    sliders[1]->value = color.g;
    sliders[2]->value = color.b;
    sliders[3]->value = color.a;
    sliders[0]->gradientStops = { Color{ 0, color.g, color.b, 255 }, Color{ 255, color.g, color.b, 255 } };
    sliders[1]->gradientStops = { Color{ color.r, 0, color.b, 255 }, Color{ color.r, 255, color.b, 255 } };
    sliders[2]->gradientStops = { Color{ color.r, color.g, 0, 255 }, Color{ color.r, color.g, 255, 255 } };
    sliders[3]->gradientStops = { Color{ color.r, color.g, color.b, 0 }, Color{ color.r, color.g, color.b, 255 } };
    lastSynced = color;
    synced = true;
}

void ColorPickerElement::OnTickSelf(float, Rectangle, float) {
    if (!swatch) return;
    sliders[3]->visible = showAlpha;
    if (!synced || !SameColor(color, lastSynced)) SyncParts();
}

Rectangle ColorPickerElement::ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const {
    float swatchHeight = kSwatchHeight * uiScale;
    float rowHeight = kRowHeight * uiScale;
    float gap = kGap * uiScale;
    if (childIndex == 0) return Rectangle{ ownRect.x, ownRect.y, ownRect.width, swatchHeight };
    float y = ownRect.y + swatchHeight + gap + (float)(childIndex - 1) * (rowHeight + gap);
    return Rectangle{ ownRect.x, y, ownRect.width, rowHeight };
}
