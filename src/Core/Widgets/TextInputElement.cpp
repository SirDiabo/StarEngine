#include "TextInputElement.h"
#include "Widget.h"
#include "Core/Utf8.h"
#include "raylib.h"

void TextInputElement::BeginEditing() {
    isEditing = true;
    textAtEditStart = text;
    cursorPos = (int)text.size();
    cursorBlinkTimer = 0.0f;
}

void TextInputElement::CommitEditing() {
    isEditing = false;
    if (onTextCommitted) onTextCommitted(text);
}

void TextInputElement::CancelEditing() {
    text = textAtEditStart;
    isEditing = false;
}

void TextInputElement::OnConfirmed() {
    WidgetElement::OnConfirmed();
    if (!disabled && !isEditing) BeginEditing();
}

void TextInputElement::OnDeselected() {
    if (isEditing) CommitEditing();
    WidgetElement::OnDeselected();
}

void TextInputElement::OnTickSelf(float deltaTime, Rectangle rect, float uiScale) {
    if (!isEditing) return;

    cursorBlinkTimer += deltaTime;
    if (cursorBlinkTimer > 1.0f) cursorBlinkTimer -= 1.0f;

    bool changed = false;
    int codepoint;
    while ((codepoint = GetCharPressed()) != 0) {
        if ((int)text.size() >= maxLength) break;
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
        }
        cursorBlinkTimer = 0.0f;
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        if (cursorPos < (int)text.size()) {
            cursorPos++;
            while (cursorPos < (int)text.size() && (text[cursorPos] & 0xC0) == 0x80) cursorPos++;
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

void TextInputElement::OnDrawSelf(Rectangle rect, float uiScale) {
    DrawRectangleRec(rect, backgroundColor);
    Color border = isEditing ? editingBorderColor : borderColor;
    DrawRectangleLinesEx(rect, 1.0f, border);

    float scaledSize = fontSize * uiScale;
    float spacing = scaledSize / 10.0f;
    float padding = 4.0f * uiScale;

    BeginScissorMode((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);

    if (text.empty() && !isEditing && !placeholder.empty()) {
        DrawTextEx(GetFontDefault(), placeholder.c_str(), {rect.x + padding, rect.y + (rect.height - scaledSize) * 0.5f},
                   scaledSize, spacing, placeholderColor);
    } else {
        DrawTextEx(GetFontDefault(), text.c_str(), {rect.x + padding, rect.y + (rect.height - scaledSize) * 0.5f},
                   scaledSize, spacing, color);
    }

    if (isEditing && cursorBlinkTimer < 0.5f) {
        std::string beforeCursor = text.substr(0, cursorPos);
        Vector2 measured = MeasureTextEx(GetFontDefault(), beforeCursor.c_str(), scaledSize, spacing);
        float cursorX = rect.x + padding + measured.x;
        DrawLineEx({cursorX, rect.y + 3.0f * uiScale}, {cursorX, rect.y + rect.height - 3.0f * uiScale}, 1.0f, color);
    }

    EndScissorMode();
}
