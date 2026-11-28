#pragma once
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

class WidgetElement;

struct PopupWindowOptions {
    bool modal = true;
    bool closable = true;
    bool movable = true;
    bool escapeCloses = true;
};

class PopupWindow {
public:
    static constexpr float kHeaderHeight = 22.0f;

    using Options = PopupWindowOptions;

    struct Frame {
        Rectangle area{};
        Rectangle content{};
        bool closeRequested = false;
    };

    Frame Draw(const std::string& title, Vector2 contentSize, const Options& options);
    void ResetPosition();

    static void OpenOverlay(const std::string& title, WidgetElement* content, Vector2 contentSize,
                            std::function<void()> onClosed, std::function<void()> onCloseRequested,
                            const Options& options = Options());

    static std::vector<std::string> WrapText(const std::string& text, int fontSize, float maxWidth);

private:
    Vector2 position{};
    bool placed = false;
    bool dragging = false;
    Vector2 dragOffset{};
    int framesDrawn = 0;
};
