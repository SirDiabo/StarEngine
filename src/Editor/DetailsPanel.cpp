#include "DetailsPanel.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"

namespace {

bool SameColor(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

const PropertyRow* FindRowById(const std::vector<PropertyRow>& rows, const std::string& id) {
    for (const PropertyRow& row : rows) {
        if (row.id == id) return &row;
    }
    return nullptr;
}

}

void DetailsPanel::Initialize() {
    name = "Details";
}

DetailsPanel::~DetailsPanel() {
    if (colorPicker.IsOpen()) colorPicker.Close();
    delete chrome;
    delete panelElement;
}

void DetailsPanel::Render() {
    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(bounds, theme.panelBg);
    DrawRectangleLinesEx(bounds, 1, theme.panelBorder);

    if (!chrome) {
        chrome = new Widget();
        panelElement = new PropertyPanel();
        chrome->SetRoot(panelElement->GetRootElement());
        panelElement->onAssetFieldClicked = [this](const std::string& rowId, const std::string& ext, Vector2 pos) {
            pendingAssetRowId = rowId;
            assetPicker.Open(pos, ext);
        };
        panelElement->onColorFieldClicked = [this](const std::string& rowId, Color current, Rectangle swatchRect) {
            if (colorPicker.IsOpen()) return;
            pendingColorRowId = rowId;
            colorBeforeEdit = current;
            lastAppliedColor = current;
            float x = swatchRect.x - ColorPickerPopup::kPopupWidth - 8.0f;
            if (x < 0.0f) x = swatchRect.x + swatchRect.width + 8.0f;
            colorPicker.Open({ x, swatchRect.y }, current);
        };
    }

    if (!buildRows) {
        DrawText("Nothing selected", (int)bounds.x + 8, (int)bounds.y + 8, 11, theme.textSecondary);
        return;
    }

    std::vector<PropertyRow> rows = buildRows();
    if (rows.empty()) {
        DrawText("Nothing selected", (int)bounds.x + 8, (int)bounds.y + 8, 11, theme.textSecondary);
    }

    panelElement->labelColor = theme.textSecondary;
    panelElement->headerColor = theme.sectionHeader;
    panelElement->readOnlyColor = { 110, 110, 118, 255 };
    panelElement->fieldBg = theme.fieldBg;
    panelElement->fieldBorder = theme.fieldBorder;
    panelElement->fieldEditingBorder = theme.accent;
    panelElement->buttonBg = theme.buttonBg;
    panelElement->buttonHover = theme.buttonHover;
    panelElement->checkBoxColor = theme.fieldBg;
    panelElement->checkBoxHoveredColor = theme.surfaceHover;
    panelElement->checkBoxBorderColor = theme.fieldBorder;
    panelElement->checkBoxCheckColor = theme.accent;
    panelElement->SetProperties(rows);

    bool pickerOpen = assetPicker.IsOpen();

    float dt = GetFrameTime();
    chrome->Tick(dt, bounds, 1.0f);
    if (!pickerOpen) {
        WidgetInput::ProcessInput(chrome, "Editor", dt, GetMousePosition(), 1.0f);
    }
    chrome->Draw(bounds, 1.0f);

    std::string picked;
    if (assetPicker.Render(picked)) {
        if (onAssetPicked) onAssetPicked(pendingAssetRowId, picked);
    }

    if (!pendingColorRowId.empty()) {
        const PropertyRow* target = FindRowById(rows, pendingColorRowId);
        bool canApply = target && target->setColor;

        if (colorPicker.IsOpen()) {
            if (!canApply) {
                colorPicker.Close();
                pendingColorRowId.clear();
            } else {
                Color live = colorPicker.GetLiveColor();
                if (!SameColor(live, lastAppliedColor)) {
                    target->setColor(live);
                    lastAppliedColor = live;
                }

                Color result;
                bool confirmed = false;
                if (colorPicker.Render(result, confirmed)) {
                    Color finalColor = confirmed ? result : colorBeforeEdit;
                    if (!SameColor(finalColor, lastAppliedColor)) target->setColor(finalColor);
                    pendingColorRowId.clear();
                }
            }
        } else {
            if (canApply && !SameColor(lastAppliedColor, colorBeforeEdit)) target->setColor(colorBeforeEdit);
            pendingColorRowId.clear();
        }
    }

    if (onRenderExtras) onRenderExtras();
}
