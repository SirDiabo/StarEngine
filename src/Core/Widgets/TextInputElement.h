#pragma once
#include "TextElement.h"
#include <functional>

class TextInputElement : public TextElement {
public:
    TextInputElement() { selectable = true; }

    const char* GetClassName() const override { return "TextInputElement"; }
    std::string GetObjectKind() const override { return "TextInputElement"; }

    Color backgroundColor{ 35, 35, 40, 255 };
    Color borderColor{ 80, 80, 85, 255 };
    Color editingBorderColor{ 90, 140, 220, 255 };
    Color placeholderColor{ 140, 140, 145, 255 };
    std::string placeholder;
    int maxLength = 256;
    bool isEditing = false;

    std::function<void(const std::string&)> onTextChanged;
    std::function<void(const std::string&)> onTextCommitted;

    void OnConfirmed() override;
    void OnDeselected() override;

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
    void OnTickSelf(float deltaTime, Rectangle rect, float uiScale) override;

private:
    void BeginEditing();
    void CommitEditing();
    void CancelEditing();

    int cursorPos = 0;
    float cursorBlinkTimer = 0.0f;
    std::string textAtEditStart;
};
