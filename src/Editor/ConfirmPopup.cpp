#include "ConfirmPopup.h"
#include "UITheme.h"
#include "Core/Widgets/WidgetOverlay.h"
#include "Core/Widgets/RectangleElement.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/ButtonElement.h"

void ConfirmPopup::Open(const std::string& msg, const std::string& confirm) {
    open = true;
    hasPending = false;

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float w = 340.0f;
    float h = 100.0f;
    area = { (sw - w) * 0.5f, (sh - h) * 0.5f, w, h };

    const UIPalette& theme = UITheme::Get();

    auto* panel = new RectangleElement();
    panel->color = theme.panelBg;

    auto* messageText = new TextElement();
    messageText->text = msg;
    messageText->color = theme.textPrimary;
    messageText->fontSize = 13.0f;
    messageText->anchor = {0.5f, 0.0f};
    messageText->pivot = {0.5f, 0.0f};
    messageText->offset = {0.0f, 18.0f};
    messageText->size = {w - 32.0f, 18.0f};
    panel->AddChild(messageText);

    float btnW = w * 0.5f - 24.0f;

    auto* cancelBtn = new ButtonElement();
    cancelBtn->defaultColor = theme.buttonBg;
    cancelBtn->hoveredColor = theme.buttonHover;
    cancelBtn->selectedColor = theme.buttonHover;
    cancelBtn->pressedColor = theme.buttonHover;
    cancelBtn->anchor = {0.0f, 0.0f};
    cancelBtn->pivot = {0.0f, 0.0f};
    cancelBtn->offset = {16.0f, h - 36.0f};
    cancelBtn->size = {btnW, 24.0f};
    cancelBtn->onActivated = [this]() {
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
    confirmBtn->defaultColor = theme.dangerBg;
    confirmBtn->hoveredColor = theme.danger;
    confirmBtn->selectedColor = theme.danger;
    confirmBtn->pressedColor = theme.danger;
    confirmBtn->anchor = {0.0f, 0.0f};
    confirmBtn->pivot = {0.0f, 0.0f};
    confirmBtn->offset = {w * 0.5f + 8.0f, h - 36.0f};
    confirmBtn->size = {btnW, 24.0f};
    confirmBtn->onActivated = [this]() {
        pendingConfirmed = true;
        hasPending = true;
    };
    auto* confirmLabel = new TextElement();
    confirmLabel->text = confirm;
    confirmLabel->color = WHITE;
    confirmLabel->fontSize = 12.0f;
    confirmLabel->anchor = {0.5f, 0.5f};
    confirmLabel->pivot = {0.5f, 0.5f};
    confirmBtn->AddChild(confirmLabel);
    panel->AddChild(confirmBtn);

    WidgetOverlay::Open(panel, { area.x, area.y }, { area.width, area.height }, [this]() {
        open = false;
    }, true);
}

void ConfirmPopup::Close() {
    open = false;
    WidgetOverlay::Close();
}

bool ConfirmPopup::Render(bool& outConfirmed) {
    if (!open) return false;
    if (hasPending) {
        outConfirmed = pendingConfirmed;
        hasPending = false;
        Close();
        return true;
    }
    return false;
}

void ConfirmPopup::FlushDeferredDraws() {
}
