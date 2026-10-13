#include "Core/KeyLayout.h"
#include "EditorUndo.h"
#include "Core/UndoManager.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"

namespace {

bool CtrlDown() {
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)
        || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
}

bool ShiftDown() {
    return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
}

bool AnyMouseDown() {
    return IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
}

bool AnyInputActivity() {
    for (int b = 0; b < 3; b++) {
        if (IsMouseButtonPressed(b) || IsMouseButtonReleased(b)) return true;
    }
    if (GetMouseWheelMove() != 0.0f) return true;
    for (int key = KEY_SPACE; key <= KEY_KB_MENU; key++) {
        if (IsKeyPressed(key) || IsKeyReleased(key)) return true;
    }
    return false;
}

bool pendingCheck = false;
std::function<void(EditorTab*)> tabRevealer;
std::function<EditorTab*()> activeTabQuery;

}

namespace EditorUndo {

void SetTabRevealer(std::function<void(EditorTab*)> revealer) {
    tabRevealer = std::move(revealer);
}

void SetActiveTabQuery(std::function<EditorTab*()> query) {
    activeTabQuery = std::move(query);
}

bool IsTabActive(const EditorTab* tab) {
    return !activeTabQuery || activeTabQuery() == tab;
}

void RevealTab(EditorTab* tab) {
    if (tab && tabRevealer) tabRevealer(tab);
}

bool PerformUndo() {
    pendingCheck = false;
    return UndoManager::Get().Undo();
}

bool PerformRedo() {
    pendingCheck = false;
    return UndoManager::Get().Redo();
}

void Tick() {
    UndoManager& undo = UndoManager::Get();

    bool textEditing = WidgetInput::IsKeyboardCaptured();
    bool modalOpen = WidgetInput::HasModal(WidgetInput::kEditorDevice);

    if (!textEditing && !modalOpen && CtrlDown()) {
        bool zPressed = KeyLayout::PressedOrRepeat('z');
        bool yPressed = KeyLayout::PressedOrRepeat('y');
        if (zPressed && !ShiftDown()) { PerformUndo(); return; }
        if (yPressed || (zPressed && ShiftDown())) { PerformRedo(); return; }
    }

    if (AnyInputActivity()) pendingCheck = true;
    if (!pendingCheck) return;
    if (AnyMouseDown() || textEditing) return;

    pendingCheck = false;
    undo.Poll();
}

}
