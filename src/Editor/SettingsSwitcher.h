#pragma once
#include "raylib.h"
#include <string>
#include <vector>

class SettingsSwitcher {
public:
    void SetCategories(const std::vector<std::string>& names);
    Rectangle Render(Rectangle area);

    int Selected() const { return selected; }

private:
    std::vector<std::string> categories;
    int selected = 0;
};
