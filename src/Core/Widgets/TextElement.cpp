#include "TextElement.h"
#include "WidgetFont.h"
#include "Core/Utf8.h"
#include "WidgetInput.h"
#include "raylib.h"
#include <cstdio>
#include <cstdlib>

namespace {

std::string FormatDragValue(float v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", v);
    std::string s = buf;
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        size_t last = s.find_last_not_of('0');
        if (last == dot) last--;
        s.erase(last + 1);
    }
    return s;
}

}

void TextElement::BeginEditing() {
    isEditing = true;
    autoStarted = false;
    edgeNavRequest = 0;
    textAtEditStart = text;
    cursorPos = (int)text.size();
    cursorBlinkTimer = 0.0f;
}

void TextElement::CommitEditing() {
    isEditing = false;
    autoStarted = false;
    if (onTextCommitted) onTextCommitted(text);
}

void TextElement::CancelEditing() {
    text = textAtEditStart;
    isEditing = false;
    autoStarted = false;
    if (onEditCancelled) onEditCancelled();
}

void TextElement::StartEditing() {
    if (!editable) return;
    if (!isEditing) BeginEditing();
}

void TextElement::FinishEditingOnFocusLoss() {
    if (autoStarted && text == textAtEditStart) {
        isEditing = false;
        autoStarted = false;
        return;
    }
    CommitEditing();
}

void TextElement::StartEditingFromNavigation() {
    if (!editable || disabled || isEditing) return;
    BeginEditing();
    autoStarted = true;
}

int TextElement::ConsumeEdgeNavRequest() {
    int request = edgeNavRequest;
    edgeNavRequest = 0;
    return request;
}

void TextElement::OnConfirmed() {
    WidgetElement::OnConfirmed();
    if (!editable || disabled) return;
    if (!isEditing) BeginEditing();
    if (numericOnly) {
        dragging = true;
        dragStartMouseX = GetMouseX();
        dragStartValue = (float)atof(text.c_str());
    }
}

void TextElement::OnDeselected() {
    if (isEditing) FinishEditingOnFocusLoss();
    dragging = false;
    WidgetElement::OnDeselected();
}

void TextElement::OnTickSelf(float deltaTime, Rectangle rect, float uiScale) {
    selectable = editable;
    if (!editable) return;
    if (!isEditing) { dragging = false; return; }
    WidgetInput::MarkKeyboardCaptured();

    bool pressedOutside = (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
        && !CheckCollisionPointRec(GetMousePosition(), rect);
    if (pressedOutside) {
        FinishEditingOnFocusLoss();
        return;
    }

    if (dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            int dx = GetMouseX() - dragStartMouseX;
            float delta = dx * 0.1f;
            float scale = 1.0f;
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) scale = 0.1f;
            if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) scale = 10.0f;
            std::string formatted = FormatDragValue(dragStartValue + delta * scale);
            if (formatted != text) {
                text = formatted;
                cursorPos = (int)text.size();
                cursorBlinkTimer = 0.0f;
                if (onTextChanged) onTextChanged(text);
            }
        } else {
            dragging = false;
        }
    }

    cursorBlinkTimer += deltaTime;
    if (cursorBlinkTimer > 1.0f) cursorBlinkTimer -= 1.0f;

    bool changed = false;
    int codepoint;
    while ((codepoint = GetCharPressed()) != 0) {
        if ((int)text.size() >= maxLength) break;
        if (numericOnly) {
            bool isDigit = codepoint >= '0' && codepoint <= '9';
            bool isDot = codepoint == '.' && text.find('.') == std::string::npos;
            bool isMinus = codepoint == '-' && cursorPos == 0 && text.find('-') == std::string::npos;
            if (!isDigit && !isDot && !isMinus) continue;
        }
        std::string encoded = Utf8::Encode(codepoint);
        text.insert(cursorPos, encoded);
        cursorPos += (int)encoded.size();
        changed = true;
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (cursorPos > 0) {
            int prev = cursorPos - 1;
            while (prev > 0 && (text[prev] & 0xC0) == 0x80) prev--;
            text.erase(prev, cursorPos - prev);
            cursorPos = prev;
            changed = true;
        }
    }
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
        if (cursorPos < (int)text.size()) {
            int next = cursorPos + 1;
            while (next < (int)text.size() && (text[next] & 0xC0) == 0x80) next++;
            text.erase(cursorPos, next - cursorPos);
            changed = true;
        }
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
        if (cursorPos > 0) {
            cursorPos--;
            while (cursorPos > 0 && (text[cursorPos] & 0xC0) == 0x80) cursorPos--;
        } else {
            edgeNavRequest = -1;
        }
        cursorBlinkTimer = 0.0f;
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        if (cursorPos < (int)text.size()) {
            cursorPos++;
            while (cursorPos < (int)text.size() && (text[cursorPos] & 0xC0) == 0x80) cursorPos++;
        } else {
            edgeNavRequest = 1;
        }
        cursorBlinkTimer = 0.0f;
    }
    if (IsKeyPressed(KEY_HOME)) { cursorPos = 0; cursorBlinkTimer = 0.0f; }
    if (IsKeyPressed(KEY_END)) { cursorPos = (int)text.size(); cursorBlinkTimer = 0.0f; }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        CommitEditing();
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        CancelEditing();
        return;
    }

    if (changed) {
        cursorBlinkTimer = 0.0f;
        if (onTextChanged) onTextChanged(text);
    }
}

void TextElement::OnDrawSelf(Rectangle rect, float uiScale) {
    Font font = WidgetFont::Get();
    float scaledSize = fontSize * uiScale;
    float spacing = scaledSize / 10.0f;

    if (!editable) {
        Vector2 textSize = MeasureTextEx(font, text.c_str(), scaledSize, spacing);
        float drawX = rect.x + (rect.width - textSize.x) * pivot.x;
        float drawY = rect.y + (rect.height - textSize.y) * pivot.y;
        DrawTextEx(font, text.c_str(), {drawX, drawY}, scaledSize, spacing, color);
        return;
    }

    DrawRectangleRec(rect, backgroundColor);
    Color border = (isEditing || interactionState == WidgetInteractionState::Selected) ? editingBorderColor : borderColor;
    DrawRectangleLinesEx(rect, 1.0f, border);

    float padding = 4.0f * uiScale;
    BeginScissorMode((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);

    if (text.empty() && !isEditing && !placeholder.empty()) {
        DrawTextEx(font, placeholder.c_str(), {rect.x + padding, rect.y + (rect.height - scaledSize) * 0.5f},
                   scaledSize, spacing, placeholderColor);
    } else {
        DrawTextEx(font, text.c_str(), {rect.x + padding, rect.y + (rect.height - scaledSize) * 0.5f},
                   scaledSize, spacing, color);
    }

    if (isEditing && cursorBlinkTimer < 0.5f) {
        std::string beforeCursor = text.substr(0, cursorPos);
        Vector2 measured = MeasureTextEx(font, beforeCursor.c_str(), scaledSize, spacing);
        float cursorX = rect.x + padding + measured.x;
        DrawLineEx({cursorX, rect.y + 3.0f * uiScale}, {cursorX, rect.y + rect.height - 3.0f * uiScale}, 1.0f, color);
    }

    EndScissorMode();
}
