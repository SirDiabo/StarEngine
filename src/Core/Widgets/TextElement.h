#pragma once
#include "WidgetElement.h"
#include <functional>
#include <string>

class TextElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "TextElement"; }
    std::string GetObjectKind() const override { return "TextElement"; }

    std::string text = "Text";
    Color color{ 255, 255, 255, 255 };
    float fontSize = 16.0f;

    bool editable = false;
    bool numericOnly = false;
    Color backgroundColor{ 35, 35, 40, 255 };
    Color borderColor{ 80, 80, 85, 255 };
    Color editingBorderColor{ 90, 140, 220, 255 };
    Color placeholderColor{ 140, 140, 145, 255 };
    std::string placeholder;
    int maxLength = 256;
    bool isEditing = false;

    std::function<void(const std::string&)> onTextChanged;
    std::function<void(const std::string&)> onTextCommitted;
    std::function<void()> onEditCancelled;

    void StartEditing();
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

    bool dragging = false;
    int dragStartMouseX = 0;
    float dragStartValue = 0.0f;
};
