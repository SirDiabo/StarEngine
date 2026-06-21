#pragma once
#include "Core/Panel.h"
#include <string>
#include <vector>

class MenuBar {
public:
    void Initialize();
    void Render();

    int activeMenu = -1;
    bool wantsNewProject = false;
    bool wantsOpenProject = false;
    bool wantsSaveProject = false;

private:
    std::vector<std::string> menus = {"File", "Edit", "View", "Tools", "Window", "Help"};
};