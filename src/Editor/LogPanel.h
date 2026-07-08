#pragma once
#include "Core/Panel.h"
#include <string>
#include <deque>
#include <vector>

enum class LogLevel { Info, Warning, Error };

struct LogEntry {
    std::string message;
    LogLevel level;
};

class LogPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

    void AddLog(const std::string& message, LogLevel level = LogLevel::Info);

    int GetTotalLogCount() const { return totalPushed; }
    std::vector<LogEntry> GetLogsSince(int fromCount) const;

private:
    std::deque<LogEntry> logs;
    int maxLogs = 100;
    int totalPushed = 0;
};