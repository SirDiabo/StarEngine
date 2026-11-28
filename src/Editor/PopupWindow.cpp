#include "PopupWindow.h"
#include "UITheme.h"
#include "Core/Widgets/WidgetInput.h"
#include "Core/Widgets/WidgetOverlay.h"
#include <algorithm>
#include <memory>

PopupWindow::Frame PopupWindow::Draw(const std::string& title, Vector2 contentSize, const Options& options) {
    const UIPalette& theme = UITheme::Get();
    const float screenW = (float)GetScreenWidth();
    const float screenH = (float)GetScreenHeight();

    float width = std::min(contentSize.x, screenW - 8.0f);
    float height = std::min(contentSize.y + kHeaderHeight, screenH - 8.0f);

    if (!placed) {
        position = { (screenW - width) * 0.5f, (screenH - height) * 0.5f };
        placed = true;
        dragging = false;
    }

    Vector2 mouse = GetMousePosition();
    Rectangle closeRect = { position.x + width - 20.0f, position.y + 3.0f, 16.0f, 16.0f };
    Rectangle headerRect = { position.x, position.y, width, kHeaderHeight };
    bool interactive = framesDrawn > 0;

    if (options.movable && interactive) {
        bool overClose = options.closable && CheckCollisionPointRec(mouse, closeRect);
        if (!dragging && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !overClose && CheckCollisionPointRec(mouse, headerRect)) {
            dragging = true;
            dragOffset = { mouse.x - position.x, mouse.y - position.y };
        }
        if (dragging) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) position = { mouse.x - dragOffset.x, mouse.y - dragOffset.y };
            else dragging = false;
        }
    }

    position.x = std::clamp(position.x, 0.0f, std::max(0.0f, screenW - width));
    position.y = std::clamp(position.y, 0.0f, std::max(0.0f, screenH - height));

    Rectangle area = { position.x, position.y, width, height };
    headerRect = { area.x, area.y, area.width, kHeaderHeight };
    closeRect = { area.x + area.width - 20.0f, area.y + 3.0f, 16.0f, 16.0f };

    if (options.modal) DrawRectangle(0, 0, (int)screenW, (int)screenH, { 0, 0, 0, 140 });
    DrawRectangleRec(area, theme.panelBg);
    DrawRectangleLinesEx(area, 1, theme.panelBorder);
    DrawRectangleRec(headerRect, theme.buttonBg);
    DrawText(title.c_str(), (int)headerRect.x + 8, (int)(headerRect.y + (headerRect.height - 11.0f) / 2.0f), 11, theme.tabText);

    Frame frame;
    frame.area = area;
    frame.content = { area.x, area.y + kHeaderHeight, area.width, area.height - kHeaderHeight };

    if (options.closable) {
        bool hovered = CheckCollisionPointRec(mouse, closeRect);
        if (hovered) DrawRectangleRec(closeRect, theme.danger);
        Color textColor = hovered ? theme.buttonText : theme.textSecondary;
        int textWidth = MeasureText("x", 10);
        DrawText("x", (int)(closeRect.x + (closeRect.width - textWidth) / 2.0f), (int)(closeRect.y + (closeRect.height - 10.0f) / 2.0f), 10, textColor);
        if (interactive && hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) frame.closeRequested = true;
        if (interactive && options.escapeCloses && IsKeyPressed(KEY_ESCAPE) && !WidgetInput::IsKeyboardCaptured()) frame.closeRequested = true;
    }

    framesDrawn++;
    return frame;
}

void PopupWindow::ResetPosition() {
    placed = false;
    dragging = false;
    framesDrawn = 0;
}

void PopupWindow::OpenOverlay(const std::string& title, WidgetElement* content, Vector2 contentSize,
                              std::function<void()> onClosed, std::function<void()> onCloseRequested,
                              const Options& options) {
    auto window = std::make_shared<PopupWindow>();
    WidgetOverlay::OpenFramed(content, contentSize, [window, title, contentSize, options]() {
        Frame frame = window->Draw(title, contentSize, options);
        WidgetOverlay::FrameResult result;
        result.content = frame.content;
        result.closeRequested = frame.closeRequested;
        return result;
    }, std::move(onClosed), std::move(onCloseRequested));
}

std::vector<std::string> PopupWindow::WrapText(const std::string& text, int fontSize, float maxWidth) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        std::string paragraph = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        std::string current;
        size_t pos = 0;
        while (pos < paragraph.size()) {
            size_t space = paragraph.find(' ', pos);
            if (space == std::string::npos) space = paragraph.size();
            std::string word = paragraph.substr(pos, space - pos);
            pos = space + 1;
            if (word.empty()) continue;
            std::string candidate = current.empty() ? word : current + " " + word;
            if (!current.empty() && (float)MeasureText(candidate.c_str(), fontSize) > maxWidth) {
                lines.push_back(current);
                current = word;
            } else {
                current = candidate;
            }
        }
        lines.push_back(current);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return lines;
}
