#include "Editor.h"
#include "ViewportPanel.h"
#include "DetailsPanel.h"
#include "ContentBrowserPanel.h"
#include "LogPanel.h"
#include "raygui.h"

void Editor::Initialize() {
    bottomTabs = {"Logs", "Content"};

    logPanel = new LogPanel();
    logPanel->bounds = Rectangle{0, 0, 0, 0};
    panels.push_back(logPanel);

    contentPanel = new ContentBrowserPanel();
    contentPanel->bounds = Rectangle{0, 0, 0, 0};
    panels.push_back(contentPanel);

    DetailsPanel* details = new DetailsPanel();
    details->bounds = Rectangle{0, 0, 0, 0};
    panels.push_back(details);

    ViewportPanel* viewport = new ViewportPanel();
    viewport->bounds = Rectangle{0, 0, 0, 0};
    panels.push_back(viewport);

    for (auto* panel : panels) {
        panel->Initialize();
    }
}

void Editor::Render() {
    int width = GetScreenWidth();
    int height = GetScreenHeight();

    float topHeight = height - 200;
    float bottomHeight = 200;

    ViewportPanel* viewport = static_cast<ViewportPanel*>(panels[3]);
    viewport->bounds = Rectangle{0, 0, (float)(width - 300), topHeight};
    viewport->Render();

    DetailsPanel* details = static_cast<DetailsPanel*>(panels[2]);
    details->bounds = Rectangle{(float)(width - 300), 0, 300, topHeight};
    details->Render();

    Rectangle tabBarBounds = {0, (float)topHeight, (float)width, 30};
    std::vector<const char*> tabTexts = {bottomTabs[0].c_str(), bottomTabs[1].c_str()};
    int result = GuiTabBar(tabBarBounds, const_cast<char**>(tabTexts.data()), (int)bottomTabs.size(), &activeBottomTab);

    Rectangle bottomBounds = {0, (float)(topHeight + 30), (float)width, (float)bottomHeight};
    if (activeBottomTab == 0) {
        logPanel->bounds = bottomBounds;
        logPanel->Render();
    } else {
        contentPanel->bounds = bottomBounds;
        contentPanel->Render();
    }
}

LogPanel* Editor::GetLogPanel() {
    return logPanel;
}