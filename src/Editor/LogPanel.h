#pragma once
#include "Core/Panel.h"
#include "ChoicePopup.h"
#include "ConfirmPopup.h"
#include "LogFile.h"
#include <deque>
#include <mutex>
#include <string>
#include <vector>

class Widget;
class WidgetElement;
class TextElement;
class CheckBoxElement;

enum class LogLevel { Info, Warning, Error };

struct LogEntry {
    std::string message;
    LogLevel level = LogLevel::Info;
    std::string time;
    int seq = 0;
};

class LogPanel : public Panel {
public:
    LogPanel();
    ~LogPanel();

    void Initialize() override;
    void Render() override;
    float CacheRefreshInterval() const override { return 0.1f; }

    void AddLog(const std::string& message, LogLevel level = LogLevel::Info);
    void Clear();

    int GetTotalLogCount() const { return totalPushed; }
    std::vector<LogEntry> GetLogsSince(int fromCount) const;

private:
    enum class MenuAction { Live, Session, DeleteOld, OpenFolder };
    enum class LineAction { Copy, CopyAll, SelectAll };
    struct MenuItem {
        MenuAction action = MenuAction::Live;
        std::string name;
    };

    void EnsureChrome();
    void EnsureSession();
    void OpenSession(const std::string& root);
    void RebuildShown();
    void LayoutChrome();
    void UpdateChromeState();
    void PollPopups();
    void OpenSessionMenu(Rectangle anchor);
    void LoadViewedSession(const std::string& name);
    void ReturnToLive();
    void CopyShown();
    void CopySelected();
    bool HasSelection() const;
    bool IsSelectedSeq(int seq) const;
    int SelectedCount() const;
    void ClearSelection();
    void OnRowPressed(int index, bool extend);
    void OnRowDragged(int index);
    void OnRowContext(int index);
    void HandleShortcuts();
    bool FiltersActive() const;
    std::string FooterText() const;

    mutable std::mutex mutex;
    std::deque<LogEntry> logs;
    std::vector<LogEntry> viewed;
    std::vector<LogEntry> shown;
    int maxLogs = 2000;
    int totalPushed = 0;
    int levelCounts[3] = { 0, 0, 0 };
    unsigned long long version = 1;
    unsigned long long shownVersion = 0;

    bool showLevel[3] = { true, true, true };
    std::string search;
    std::string searchLower;
    bool follow = true;
    bool shownDirty = true;

    std::string viewedName;
    bool viewedTruncated = false;

    LogFile::Writer writer;
    std::string sessionRoot;
    bool sessionOpened = false;
    bool backlogWritten = false;
    std::string fileError;

    double copiedUntil = 0.0;
    int selectionAnchor = -1;
    int selectionActive = -1;

    Widget* chrome = nullptr;
    WidgetElement* chromeRoot = nullptr;
    WidgetElement* clearButton = nullptr;
    WidgetElement* copyButton = nullptr;
    WidgetElement* sessionButton = nullptr;
    WidgetElement* levelButtons[3] = { nullptr, nullptr, nullptr };
    TextElement* searchField = nullptr;
    CheckBoxElement* followBox = nullptr;
    TextElement* followLabel = nullptr;
    WidgetElement* listElement = nullptr;
    TextElement* footer = nullptr;

    ChoicePopup sessionMenu;
    std::vector<MenuItem> menuItems;
    ConfirmPopup deleteConfirm;
    ChoicePopup lineMenu;
    std::vector<LineAction> lineMenuActions;
};
