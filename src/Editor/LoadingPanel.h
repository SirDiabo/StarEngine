#pragma once
#include "raylib.h"
#include "PopupWindow.h"
#include <string>

class LoadingPanel {
public:
    static LoadingPanel& Get();

    void Show(const std::string& title);
    void Hide();
    bool IsVisible() const { return visible; }

    void SetStatusLine(const std::string& line);

    void Render();

private:
    bool visible = false;
    PopupWindow window;
    std::string title;
    std::string statusLine;
};
