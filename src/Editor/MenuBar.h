#pragma once
#include <string>
#include <vector>
#include <functional>

struct MenuEntry {
    std::string label;
    std::vector<std::string> items;
    
    std::vector<bool> checked;
};

// A menu action callback: receives the menu label and item label that was clicked.
using MenuActionFn = std::function<void(const std::string& menu, const std::string& item)>;

class MenuBar {
public:
    void Initialize();
    // Draw the bar background and buttons. Call before panel content.
    void RenderBar();
    // Draw the open popup on top of everything. Call after all panel content.
    void RenderPopup();

    // Replace the current menu set (called by the active EditorTab).
    void SetMenus(const std::vector<MenuEntry>& entries);

    // Register a handler that receives clicked menu items.
    void SetActionCallback(MenuActionFn fn);

    // Convenience flags checked by Editor for top-level project actions.
    bool wantsNewProject  = false;
    bool wantsOpenProject = false;
    bool wantsSaveProject = false;

    int activeMenu = -1;

private:
    float GetMenuButtonX(int index) const;
    std::vector<MenuEntry> menus;
    MenuActionFn onAction;
    float menuBarHeight = 20.0f;
};