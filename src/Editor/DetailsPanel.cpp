#include "DetailsPanel.h"
#include "UITheme.h"
#include "UIWidgets.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"

void DetailsPanel::Initialize() {
    name = "Details";
}

DetailsPanel::~DetailsPanel() {
    delete chrome;
    delete panelElement;
}

void DetailsPanel::Render() {
    const UIPalette& theme = UITheme::Get();
    UIWidget::Panel(bounds);

    if (!chrome) {
        chrome = new Widget();
        panelElement = new PropertyPanel();
        chrome->SetRoot(panelElement->GetRootElement());
        panelElement->onAssetFieldClicked = [this](const std::string& rowId, const std::string& ext, Vector2 pos) {
            pendingAssetRowId = rowId;
            assetPicker.Open(pos, ext);
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
}
