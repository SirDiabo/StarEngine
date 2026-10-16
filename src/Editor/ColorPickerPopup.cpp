#include "ColorPickerPopup.h"
#include "UITheme.h"
#include "Core/Widgets/WidgetOverlay.h"
#include "Core/Widgets/SliderElement.h"
#include "Core/Widgets/RectangleElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/GridElement.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

namespace {

const float POPUP_W = ColorPickerPopup::kPopupWidth;
const float POPUP_H = 415.0f;

struct PresetSwatch {
    const char* name;
    Color color;
};

const PresetSwatch kPresetSwatches[] = {
    {"Red",    {225, 60, 60, 255}},
    {"Orange", {230, 140, 50, 255}},
    {"Yellow", {225, 210, 60, 255}},
    {"Green",  {70, 175, 80, 255}},
    {"Blue",   {70, 120, 220, 255}},
    {"Purple", {150, 80, 200, 255}},
    {"Cyan",   {60, 190, 200, 255}},
    {"Lime",   {150, 220, 60, 255}},
    {"Brown",  {140, 95, 60, 255}},
    {"White",  {255, 255, 255, 255}},
    {"Black",  {20, 20, 20, 255}},
    {"Gray",   {130, 130, 130, 255}},
};
const int kPresetSwatchCount = sizeof(kPresetSwatches) / sizeof(kPresetSwatches[0]);

struct SliderRow {
    SliderElement* slider;
    TextElement* valueText;
};

SliderRow BuildSliderRow(RectangleElement* parent, const std::string& labelText, float y, float contentW,
                          const UIPalette& theme, std::function<void(float)> onChanged) {
    auto* label = new TextElement();
    label->text = labelText;
    label->color = theme.textSecondary;
    label->fontSize = 11.0f;
    label->anchor = {0.0f, 0.0f};
    label->pivot = {0.0f, 0.0f};
    label->offset = {10.0f, y + 3.0f};
    label->size = {14.0f, 14.0f};
    parent->AddChild(label);

    auto* slider = new SliderElement();
    slider->anchor = {0.0f, 0.0f};
    slider->pivot = {0.0f, 0.0f};
    slider->offset = {24.0f, y};
    slider->size = {contentW - 24.0f - 40.0f, 18.0f};
    slider->borderColor = theme.fieldBorder;
    slider->onValueChanged = onChanged;
    parent->AddChild(slider);

    auto* valueText = new TextElement();
    valueText->color = theme.textPrimary;
    valueText->fontSize = 11.0f;
    valueText->anchor = {0.0f, 0.0f};
    valueText->pivot = {0.0f, 0.0f};
    valueText->offset = {10.0f + contentW - 40.0f + 4.0f, y + 3.0f};
    valueText->size = {40.0f, 14.0f};
    parent->AddChild(valueText);

    return { slider, valueText };
}

}

void ColorPickerPopup::Open(Vector2 anchorPos, Color startColor) {
    open = true;
    hasPending = false;

    original = startColor;
    current = startColor;
    rf = startColor.r / 255.0f;
    gf = startColor.g / 255.0f;
    bf = startColor.b / 255.0f;
    af = startColor.a / 255.0f;

    Vector3 hsv = ColorToHSV(startColor);
    hh = hsv.x;
    ss = hsv.y;
    vv = hsv.z;
    snprintf(hexBuf, sizeof(hexBuf), "%02X%02X%02X%02X", current.r, current.g, current.b, current.a);

    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float x = anchorPos.x;
    float y = anchorPos.y;
    if (x + POPUP_W > screenW) x = screenW - POPUP_W - 4;
    if (y + POPUP_H > screenH) y = screenH - POPUP_H - 4;
    area = { x, y, POPUP_W, POPUP_H };

    const UIPalette& theme = UITheme::Get();

    auto* panel = new RectangleElement();
    panel->color = theme.panelBg;

    float contentW = POPUP_W - 20.0f;
    float fy = 10.0f;

    auto* oldLabel = new TextElement();
    oldLabel->text = "Old";
    oldLabel->color = theme.textSecondary;
    oldLabel->fontSize = 10.0f;
    oldLabel->anchor = {0.0f, 0.0f};
    oldLabel->pivot = {0.0f, 0.0f};
    oldLabel->offset = {10.0f, fy};
    oldLabel->size = {60.0f, 12.0f};
    panel->AddChild(oldLabel);

    float swatchW = (contentW - 8.0f) / 2.0f;
    auto* newLabel = new TextElement();
    newLabel->text = "New";
    newLabel->color = theme.textSecondary;
    newLabel->fontSize = 10.0f;
    newLabel->anchor = {0.0f, 0.0f};
    newLabel->pivot = {0.0f, 0.0f};
    newLabel->offset = {10.0f + swatchW + 8.0f, fy};
    newLabel->size = {60.0f, 12.0f};
    panel->AddChild(newLabel);
    fy += 12.0f;

    auto* oldSwatch = new RectangleElement();
    oldSwatch->color = original;
    oldSwatch->anchor = {0.0f, 0.0f};
    oldSwatch->pivot = {0.0f, 0.0f};
    oldSwatch->offset = {10.0f, fy};
    oldSwatch->size = {swatchW, 34.0f};
    panel->AddChild(oldSwatch);

    newSwatchEl = new RectangleElement();
    newSwatchEl->color = current;
    newSwatchEl->anchor = {0.0f, 0.0f};
    newSwatchEl->pivot = {0.0f, 0.0f};
    newSwatchEl->offset = {10.0f + swatchW + 8.0f, fy};
    newSwatchEl->size = {swatchW, 34.0f};
    panel->AddChild(newSwatchEl);
    fy += 34.0f + 10.0f;

    auto rRow = BuildSliderRow(panel, "R", fy, contentW, theme, [this](float v) { rf = v; SyncFromRGB(); RefreshControls(); });
    rSlider = rRow.slider; rValueText = rRow.valueText;
    fy += 22.0f;

    auto gRow = BuildSliderRow(panel, "G", fy, contentW, theme, [this](float v) { gf = v; SyncFromRGB(); RefreshControls(); });
    gSlider = gRow.slider; gValueText = gRow.valueText;
    fy += 22.0f;

    auto bRow = BuildSliderRow(panel, "B", fy, contentW, theme, [this](float v) { bf = v; SyncFromRGB(); RefreshControls(); });
    bSlider = bRow.slider; bValueText = bRow.valueText;
    fy += 22.0f;

    auto aRow = BuildSliderRow(panel, "A", fy, contentW, theme, [this](float v) { af = v; SyncFromRGB(); RefreshControls(); });
    aSlider = aRow.slider; aValueText = aRow.valueText;
    fy += 26.0f;

    auto hRow = BuildSliderRow(panel, "H", fy, contentW, theme, [this](float v) { hh = v; SyncFromHSV(); RefreshControls(); });
    hSlider = hRow.slider; hValueText = hRow.valueText;
    hSlider->minValue = 0.0f;
    hSlider->maxValue = 360.0f;
    hSlider->gradientStops = {
        {255, 0, 0, 255}, {255, 255, 0, 255}, {0, 255, 0, 255}, {0, 255, 255, 255},
        {0, 0, 255, 255}, {255, 0, 255, 255}, {255, 0, 0, 255},
    };
    fy += 22.0f;

    auto sRow = BuildSliderRow(panel, "S", fy, contentW, theme, [this](float v) { ss = v; SyncFromHSV(); RefreshControls(); });
    sSlider = sRow.slider; sValueText = sRow.valueText;
    fy += 22.0f;

    auto vRow = BuildSliderRow(panel, "V", fy, contentW, theme, [this](float v) { vv = v; SyncFromHSV(); RefreshControls(); });
    vSlider = vRow.slider; vValueText = vRow.valueText;
    fy += 28.0f;

    auto* hexLabel = new TextElement();
    hexLabel->text = "Hex";
    hexLabel->color = theme.textSecondary;
    hexLabel->fontSize = 11.0f;
    hexLabel->anchor = {0.0f, 0.0f};
    hexLabel->pivot = {0.0f, 0.0f};
    hexLabel->offset = {10.0f, fy + 4.0f};
    hexLabel->size = {30.0f, 14.0f};
    panel->AddChild(hexLabel);

    hexBox = new TextElement();
    hexBox->editable = true;
    hexBox->text = hexBuf;
    hexBox->fontSize = 11.0f;
    hexBox->color = theme.textPrimary;
    hexBox->backgroundColor = theme.fieldBg;
    hexBox->borderColor = theme.fieldBorder;
    hexBox->editingBorderColor = theme.accent;
    hexBox->anchor = {0.0f, 0.0f};
    hexBox->pivot = {0.0f, 0.0f};
    hexBox->offset = {42.0f, fy};
    hexBox->size = {contentW - 32.0f, 20.0f};
    hexBox->onTextCommitted = [this](const std::string& text) {
        strncpy(hexBuf, text.c_str(), sizeof(hexBuf) - 1);
        hexBuf[sizeof(hexBuf) - 1] = 0;
        SyncFromHex();
        RefreshControls();
    };
    panel->AddChild(hexBox);
    fy += 30.0f;

    auto* presetsLabel = new TextElement();
    presetsLabel->text = "Presets";
    presetsLabel->color = theme.textSecondary;
    presetsLabel->fontSize = 10.0f;
    presetsLabel->anchor = {0.0f, 0.0f};
    presetsLabel->pivot = {0.0f, 0.0f};
    presetsLabel->offset = {10.0f, fy};
    presetsLabel->size = {80.0f, 12.0f};
    panel->AddChild(presetsLabel);
    fy += 14.0f;

    auto* grid = new GridElement();
    grid->columns = 6;
    grid->columnWidth = 34.0f;
    grid->rowHeight = 34.0f;
    grid->cellSpacing = 4.0f;
    grid->anchor = {0.0f, 0.0f};
    grid->pivot = {0.0f, 0.0f};
    grid->offset = {10.0f, fy};
    int swRows = (kPresetSwatchCount + 5) / 6;
    grid->size = {contentW, (float)swRows * (34.0f + 4.0f)};
    for (int i = 0; i < kPresetSwatchCount; i++) {
        auto* swatchBtn = new ButtonElement();
        Color c = kPresetSwatches[i].color;
        swatchBtn->defaultColor = c;
        swatchBtn->hoveredColor = c;
        swatchBtn->selectedColor = c;
        swatchBtn->pressedColor = c;
        swatchBtn->size = {0.0f, 0.0f};
        swatchBtn->onActivated = [this, c]() {
            rf = c.r / 255.0f;
            gf = c.g / 255.0f;
            bf = c.b / 255.0f;
            SyncFromRGB();
            RefreshControls();
        };
        grid->AddChild(swatchBtn);
    }
    panel->AddChild(grid);
    fy += (float)swRows * (34.0f + 4.0f) + 8.0f;

    float btnW = (contentW - 8.0f) / 2.0f;
    auto* cancelBtn = new ButtonElement();
    cancelBtn->defaultColor = theme.buttonBg;
    cancelBtn->hoveredColor = theme.buttonHover;
    cancelBtn->selectedColor = theme.buttonHover;
    cancelBtn->pressedColor = theme.buttonPressed;
    cancelBtn->anchor = {0.0f, 0.0f};
    cancelBtn->pivot = {0.0f, 0.0f};
    cancelBtn->offset = {10.0f, POPUP_H - 34.0f};
    cancelBtn->size = {btnW, 24.0f};
    cancelBtn->onActivated = [this]() {
        pendingColor = original;
        pendingConfirmed = false;
        hasPending = true;
    };
    auto* cancelLabel = new TextElement();
    cancelLabel->text = "Cancel";
    cancelLabel->color = theme.buttonText;
    cancelLabel->fontSize = 12.0f;
    cancelLabel->anchor = {0.5f, 0.5f};
    cancelLabel->pivot = {0.5f, 0.5f};
    cancelBtn->AddChild(cancelLabel);
    panel->AddChild(cancelBtn);

    auto* confirmBtn = new ButtonElement();
    confirmBtn->defaultColor = theme.buttonBg;
    confirmBtn->hoveredColor = theme.buttonHover;
    confirmBtn->selectedColor = theme.buttonHover;
    confirmBtn->pressedColor = theme.buttonPressed;
    confirmBtn->anchor = {0.0f, 0.0f};
    confirmBtn->pivot = {0.0f, 0.0f};
    confirmBtn->offset = {10.0f + btnW + 8.0f, POPUP_H - 34.0f};
    confirmBtn->size = {btnW, 24.0f};
    confirmBtn->onActivated = [this]() {
        pendingColor = current;
        pendingConfirmed = true;
        hasPending = true;
    };
    auto* confirmLabel = new TextElement();
    confirmLabel->text = "Confirm";
    confirmLabel->color = theme.buttonText;
    confirmLabel->fontSize = 12.0f;
    confirmLabel->anchor = {0.5f, 0.5f};
    confirmLabel->pivot = {0.5f, 0.5f};
    confirmBtn->AddChild(confirmLabel);
    panel->AddChild(confirmBtn);

    RefreshControls();

    WidgetOverlay::Open(panel, { area.x, area.y }, { area.width, area.height }, [this]() {
        open = false;
    });
}

void ColorPickerPopup::Close() {
    open = false;
    WidgetOverlay::Close();
}

bool ColorPickerPopup::ContainsPoint(Vector2 point) const {
    return open && CheckCollisionPointRec(point, area);
}

void ColorPickerPopup::SyncFromRGB() {
    current = {(unsigned char)(rf * 255.0f), (unsigned char)(gf * 255.0f),
               (unsigned char)(bf * 255.0f), (unsigned char)(af * 255.0f)};
    Vector3 hsv = ColorToHSV(current);
    hh = hsv.x;
    ss = hsv.y;
    vv = hsv.z;
    snprintf(hexBuf, sizeof(hexBuf), "%02X%02X%02X%02X", current.r, current.g, current.b, current.a);
}

void ColorPickerPopup::SyncFromHSV() {
    Color rgb = ColorFromHSV(hh, ss, vv);
    current = {rgb.r, rgb.g, rgb.b, (unsigned char)(af * 255.0f)};
    rf = current.r / 255.0f;
    gf = current.g / 255.0f;
    bf = current.b / 255.0f;
    snprintf(hexBuf, sizeof(hexBuf), "%02X%02X%02X%02X", current.r, current.g, current.b, current.a);
}

void ColorPickerPopup::SyncFromHex() {
    unsigned long value = strtoul(hexBuf, nullptr, 16);
    size_t len = strlen(hexBuf);

    unsigned char r, g, b, a;
    if (len <= 6) {
        r = (unsigned char)((value >> 16) & 0xFF);
        g = (unsigned char)((value >> 8) & 0xFF);
        b = (unsigned char)(value & 0xFF);
        a = 255;
    } else {
        r = (unsigned char)((value >> 24) & 0xFF);
        g = (unsigned char)((value >> 16) & 0xFF);
        b = (unsigned char)((value >> 8) & 0xFF);
        a = (unsigned char)(value & 0xFF);
    }

    current = {r, g, b, a};
    rf = r / 255.0f;
    gf = g / 255.0f;
    bf = b / 255.0f;
    af = a / 255.0f;
    Vector3 hsv = ColorToHSV(current);
    hh = hsv.x;
    ss = hsv.y;
    vv = hsv.z;
}

void ColorPickerPopup::RefreshControls() {
    rSlider->value = rf;
    gSlider->value = gf;
    bSlider->value = bf;
    aSlider->value = af;
    hSlider->value = hh;
    sSlider->value = ss;
    vSlider->value = vv;

    rSlider->gradientStops = { Color{0, (unsigned char)(gf * 255), (unsigned char)(bf * 255), 255},
                                Color{255, (unsigned char)(gf * 255), (unsigned char)(bf * 255), 255} };
    gSlider->gradientStops = { Color{(unsigned char)(rf * 255), 0, (unsigned char)(bf * 255), 255},
                                Color{(unsigned char)(rf * 255), 255, (unsigned char)(bf * 255), 255} };
    bSlider->gradientStops = { Color{(unsigned char)(rf * 255), (unsigned char)(gf * 255), 0, 255},
                                Color{(unsigned char)(rf * 255), (unsigned char)(gf * 255), 255, 255} };
    aSlider->gradientStops = { Color{(unsigned char)(rf * 255), (unsigned char)(gf * 255), (unsigned char)(bf * 255), 0},
                                Color{(unsigned char)(rf * 255), (unsigned char)(gf * 255), (unsigned char)(bf * 255), 255} };
    sSlider->gradientStops = { ColorFromHSV(hh, 0.0f, vv), ColorFromHSV(hh, 1.0f, vv) };
    vSlider->gradientStops = { ColorFromHSV(hh, ss, 0.0f), ColorFromHSV(hh, ss, 1.0f) };

    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", rf); rValueText->text = buf;
    snprintf(buf, sizeof(buf), "%.2f", gf); gValueText->text = buf;
    snprintf(buf, sizeof(buf), "%.2f", bf); bValueText->text = buf;
    snprintf(buf, sizeof(buf), "%.2f", af); aValueText->text = buf;
    snprintf(buf, sizeof(buf), "%.0f", hh); hValueText->text = buf;
    snprintf(buf, sizeof(buf), "%.2f", ss); sValueText->text = buf;
    snprintf(buf, sizeof(buf), "%.2f", vv); vValueText->text = buf;

    newSwatchEl->color = current;
    if (!hexBox->isEditing) hexBox->text = hexBuf;
}

bool ColorPickerPopup::Render(Color& outColor, bool& outConfirmed) {
    if (!open) return false;
    if (hasPending) {
        outColor = pendingColor;
        outConfirmed = pendingConfirmed;
        hasPending = false;
        Close();
        return true;
    }
    return false;
}
