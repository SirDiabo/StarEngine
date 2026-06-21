#pragma once
#include "Core/Panel.h"
#include <vector>
#include <string>

class LogPanel;
class ContentBrowserPanel;

class Editor {
public:
    void Initialize();
    void Render();
    LogPanel* GetLogPanel();

private:
    std::vector<Panel*> panels;
    LogPanel* logPanel = nullptr;
    ContentBrowserPanel* contentPanel = nullptr;
    std::vector<std::string> bottomTabs;
    int activeBottomTab = 0;
};