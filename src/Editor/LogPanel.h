#pragma once
#include "Core/Panel.h"
#include <string>
#include <deque>

class LogPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

    void AddLog(const std::string& message);

private:
    std::deque<std::string> logs;
    int maxLogs = 100;
};