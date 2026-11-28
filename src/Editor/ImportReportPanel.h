#pragma once
#include "Core/ImportHandlerRegistry.h"
#include "PopupWindow.h"
#include <string>
#include <vector>

class ImportReportPanel {
public:
    static ImportReportPanel& Get();

    void Open(const std::string& title, const std::vector<ImportMessage>& messages);
    void Close();
    bool IsOpen() const { return open; }

    void Render();

private:
    bool open = false;
    bool preventClose = false;
    PopupWindow window;
    float scroll = 0.0f;
    std::string title;
    std::vector<ImportMessage> messages;
};
