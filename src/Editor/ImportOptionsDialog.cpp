#include "ImportOptionsDialog.h"
#include "PropertyPanel.h"
#include "PropertyRowBuilders.h"
#include "UITheme.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

std::string FormatNumber(float value, bool integer) {
    char buffer[32];
    if (integer) snprintf(buffer, sizeof(buffer), "%d", (int)value);
    else snprintf(buffer, sizeof(buffer), "%g", value);
    return buffer;
}

}

ImportOptionsDialog& ImportOptionsDialog::Get() {
    static ImportOptionsDialog instance;
    return instance;
}

void ImportOptionsDialog::Enqueue(const ImportHandler& handler, const std::string& sourcePath, ConfirmCallback onConfirm) {
    queue.push_back({ handler, sourcePath, std::move(onConfirm) });
    if (!active) Activate();
}

void ImportOptionsDialog::Activate() {
    if (queue.empty()) {
        active = false;
        return;
    }
    current = std::move(queue.front());
    queue.pop_front();
    values = ImportHandlerRegistry::DefaultOptions(current.handler);
    auto saved = remembered.find(current.handler.extension);
    if (saved != remembered.end()) {
        for (const auto& entry : saved->second) {
            if (values.count(entry.first)) values[entry.first] = entry.second;
        }
    }
    active = true;
}

void ImportOptionsDialog::Finish(bool confirmed) {
    WidgetInput::PopModal(this, WidgetInput::kEditorDevice);
    WidgetInput::SetInputScope(nullptr);
    Request finished = std::move(current);
    std::map<std::string, std::string> finalValues = values;
    if (confirmed) remembered[finished.handler.extension] = finalValues;
    active = false;
    if (confirmed && finished.onConfirm) finished.onConfirm(finalValues);
    Activate();
}

bool ImportOptionsDialog::IsVisible(const ImportOptionDef& def) const {
    if (def.showIfId.empty()) return true;
    auto it = values.find(def.showIfId);
    return it != values.end() && it->second == def.showIfValue;
}

std::vector<PropertyRow> ImportOptionsDialog::BuildRows() {
    using namespace PropertyRows;
    std::vector<PropertyRow> rows;
    std::string lastGroup;
    for (const ImportOptionDef& def : current.handler.options) {
        if (!IsVisible(def)) continue;
        if (def.group != lastGroup) {
            if (!def.group.empty()) rows.push_back(HeaderRow("group." + def.group, def.group));
            lastGroup = def.group;
        }
        const std::string id = def.id;
        const std::string value = values[id];
        if (def.kind == ImportOptionKind::Bool) {
            rows.push_back(BoolRow(id, def.label, value == "true", [this, id](bool v) { values[id] = v ? "true" : "false"; }));
        } else if (def.kind == ImportOptionKind::Enum) {
            rows.push_back(EnumRow(id, def.label, value, def.choices, [this, id](const std::string& v) { values[id] = v; }));
        } else {
            const bool integer = def.kind == ImportOptionKind::Int;
            float lo = def.minValue, hi = def.maxValue;
            float shown = 0.0f;
            try { shown = std::stof(value); } catch (...) { shown = lo; }
            rows.push_back(FloatRow(id, def.label, shown, integer, [this, id, integer, lo, hi](float v) {
                values[id] = FormatNumber(std::clamp(v, lo, hi), integer);
            }));
        }
    }

    PropertyRow buttons;
    buttons.id = "buttons";
    buttons.kind = PropertyRowKind::Cells;
    PropertyCell cancel;
    cancel.kind = PropertyCellKind::Button;
    cancel.text = "Cancel";
    cancel.onPressed = [this](Rectangle) { Finish(false); };
    PropertyCell confirm;
    confirm.kind = PropertyCellKind::Button;
    confirm.text = "Import";
    confirm.onPressed = [this](Rectangle) { Finish(true); };
    buttons.cells = { cancel, confirm };
    rows.push_back(buttons);
    return rows;
}

void ImportOptionsDialog::EnsureChrome() {
    if (chrome) return;
    chrome = new Widget();
    panel = new PropertyPanel();
    chrome->SetRoot(panel->GetRootElement());
}

void ImportOptionsDialog::Render() {
    if (!active) return;
    EnsureChrome();

    const UIPalette& theme = UITheme::Get();
    panel->labelColor = theme.textSecondary;
    panel->headerColor = theme.sectionHeader;
    panel->readOnlyColor = { 110, 110, 118, 255 };
    panel->fieldBg = theme.fieldBg;
    panel->fieldBorder = theme.fieldBorder;
    panel->fieldEditingBorder = theme.accent;
    panel->buttonBg = theme.buttonBg;
    panel->buttonHover = theme.buttonHover;
    panel->checkBoxColor = theme.fieldBg;
    panel->checkBoxHoveredColor = theme.surfaceHover;
    panel->checkBoxBorderColor = theme.fieldBorder;
    panel->checkBoxCheckColor = theme.accent;

    std::vector<PropertyRow> rows = BuildRows();
    panel->SetProperties(rows);

    const std::string label = current.handler.label.empty() ? std::string("Import") : current.handler.label;
    const std::string title = label + " Import - " + fs::path(current.sourcePath).stem().string();

    const float screenW = (float)GetScreenWidth();
    const float screenH = (float)GetScreenHeight();
    const float titleH = 30.0f;
    const float boxWidth = 380.0f;
    float boxHeight = titleH + (float)rows.size() * panel->rowHeight + 14.0f;
    boxHeight = std::min(boxHeight, screenH - 40.0f);
    Rectangle box = { screenW * 0.5f - boxWidth * 0.5f, screenH * 0.5f - boxHeight * 0.5f, boxWidth, boxHeight };
    Rectangle body = { box.x + 6.0f, box.y + titleH, boxWidth - 12.0f, boxHeight - titleH - 6.0f };

    DrawRectangle(0, 0, (int)screenW, (int)screenH, { 0, 0, 0, 140 });
    DrawRectangleRec(box, theme.panelBg);
    DrawRectangleLinesEx(box, 1, theme.panelBorder);
    DrawText(title.c_str(), (int)box.x + 10, (int)box.y + 8, 12, theme.sectionHeader);

    WidgetInput::PushModal(this, WidgetInput::kEditorDevice);
    WidgetInput::SetInputScope(this);
    float dt = GetFrameTime();
    chrome->Tick(dt, body, 1.0f);
    WidgetInput::ProcessInput(chrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    chrome->Draw(body, 1.0f);
    WidgetInput::SetInputScope(nullptr);

    if (active && IsKeyPressed(KEY_ESCAPE) && !WidgetInput::IsKeyboardCaptured()) Finish(false);
}
