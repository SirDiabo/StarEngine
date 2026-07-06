#include "ChoicePopup.h"

std::vector<std::function<void()>> ChoicePopup::deferredDraws;

static float RowHeight(const ChoiceOption& option) {
    return option.enabled || option.disabledReason.empty() ? 20.0f : 32.0f;
}

void ChoicePopup::Open(Vector2 anchorPos, float width, const std::vector<ChoiceOption>& choices) {
    open = true;
    justOpened = true;
    options = choices;

    float height = 4.0f;
    for (const auto& opt : options) height += RowHeight(opt);
    area = {anchorPos.x, anchorPos.y, width, height};
}

void ChoicePopup::Close() {
    open = false;
}

bool ChoicePopup::Render(int& outIndex) {
    if (!open) return false;

    Vector2 mousePos = GetMousePosition();

    Rectangle areaCopy = area;
    deferredDraws.push_back([areaCopy]() {
        DrawRectangleRec(areaCopy, {45, 45, 50, 255});
        DrawRectangleLinesEx(areaCopy, 1, {80, 80, 88, 255});
    });

    float y = area.y + 2;
    for (size_t i = 0; i < options.size(); i++) {
        const ChoiceOption& opt = options[i];
        float rowHeight = RowHeight(opt);
        Rectangle item = {area.x + 1, y, area.width - 2, rowHeight};
        bool hovered = opt.enabled && CheckCollisionPointRec(mousePos, item);

        std::string label = opt.label;
        std::string reason = opt.disabledReason;
        bool enabled = opt.enabled;
        deferredDraws.push_back([item, hovered, label, reason, enabled]() {
            if (hovered) DrawRectangleRec(item, {70, 70, 80, 255});
            Color textColor = enabled ? WHITE : Color{120, 120, 128, 255};
            DrawText(label.c_str(), (int)item.x + 6, (int)item.y + 3, 11, textColor);
            if (!enabled && !reason.empty()) {
                DrawText(reason.c_str(), (int)item.x + 6, (int)item.y + 17, 9, {110, 110, 118, 255});
            }
        });

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            outIndex = (int)i;
            Close();
            return true;
        }
        y += rowHeight;
    }

    if (justOpened) {
        justOpened = false;
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, area)) {
        Close();
    }

    return false;
}

void ChoicePopup::FlushDeferredDraws() {
    for (const auto& draw : deferredDraws) draw();
    deferredDraws.clear();
}
