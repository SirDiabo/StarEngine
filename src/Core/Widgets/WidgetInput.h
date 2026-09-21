#pragma once
#include <string>
#include "raylib.h"

class Widget;

namespace WidgetInput {

inline constexpr const char* kEditorDevice = "Editor";

void ProcessInput(Widget* widget, const std::string& device, float deltaTime, Vector2 localMousePos, float uiScale = 1.0f);

void SetInputScope(const void* owner);

void ClaimFocus(Widget* widget, const std::string& device);
void ClaimOwnerFocus(const void* owner, const std::string& device);
void ReleaseFocus(const std::string& device);
bool IsFocused(const Widget* widget, const std::string& device);
const void* GetFocusOwner(const std::string& device);

void PushModal(const void* owner, const std::string& device);
void PopModal(const void* owner, const std::string& device);
bool HasModal(const std::string& device);

void MarkKeyboardCaptured();
bool IsKeyboardCaptured();

bool ScopeHasKeyboard();
bool WidgetHasKeyboard(const Widget* widget);

void Forget(const Widget* widget);

}
