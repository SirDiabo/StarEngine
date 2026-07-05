#pragma once
#include "raylib.h"
#include <string>

class AboutPanel {
public:
    static AboutPanel& Get();

    void Open();
    void Close();
    bool IsOpen() const { return open; }

    void Render();

private:
    bool open = false;
    bool preventClose = false;
    Texture2D logo = { 0 };
    bool logoLoaded = false;
};
