#include "LogPanel.h"
#include "UITheme.h"
#include "Core/Project.h"
#include "Core/SafeProcess.h"
#include "Core/Widgets/ButtonElement.h"
#include "Core/Widgets/CheckBoxElement.h"
#include "Core/Widgets/ScissorStack.h"
#include "Core/Widgets/TextElement.h"
#include "Core/Widgets/Widget.h"
#include "Core/Widgets/WidgetInput.h"
#include "raylib.h"
#include <algorithm>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

constexpr float kToolbarHeight = 26.0f;
constexpr float kSearchHeight = 24.0f;
constexpr float kFooterHeight = 16.0f;
constexpr float kLineHeight = 14.0f;
constexpr float kPad = 4.0f;

int LevelIndex(LogLevel level) {
    return level == LogLevel::Warning ? 1 : (level == LogLevel::Error ? 2 : 0);
}

LogLevel LevelFromChar(char c) {
    return c == 'W' ? LogLevel::Warning : (c == 'E' ? LogLevel::Error : LogLevel::Info);
}

char LevelToChar(LogLevel level) {
    return level == LogLevel::Warning ? 'W' : (level == LogLevel::Error ? 'E' : 'I');
}

Color LogEntryColor(LogLevel level) {
    const UIPalette& theme = UITheme::Get();
    switch (level) {
        case LogLevel::Warning: return { 240, 200, 60, 255 };
        case LogLevel::Error:   return theme.danger;
        default:                return theme.textPrimary;
    }
}

std::string Lower(const std::string& text) {
    std::string result = text;
    for (char& c : result) c = (char)tolower((unsigned char)c);
    return result;
}

std::string SizeText(long long bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    return std::to_string(bytes / (1024 * 1024)) + " MB";
}

class LogButtonElement : public ButtonElement {
public:
    const char* GetClassName() const override { return "LogButtonElement"; }
    std::string GetObjectKind() const override { return "LogButtonElement"; }

    std::string label;
    Color textColor{ 220, 220, 225, 255 };

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override {
        ButtonElement::OnDrawSelf(rect, uiScale);
        ScissorStack::Push(rect);
        int width = MeasureText(label.c_str(), 11);
        float x = rect.x + (rect.width - (float)width) * 0.5f;
        if (x < rect.x + 4.0f) x = rect.x + 4.0f;
        DrawText(label.c_str(), (int)x, (int)(rect.y + rect.height * 0.5f - 5.0f), 11, textColor);
        ScissorStack::Pop();
    }
};

class LogListElement : public WidgetElement {
public:
    const char* GetClassName() const override { return "LogListElement"; }
    std::string GetObjectKind() const override { return "LogListElement"; }

    const std::vector<LogEntry>* rows = nullptr;
    bool* follow = nullptr;
    float scroll = 0.0f;
    Color background{ 30, 30, 34, 255 };
    Color timeColor{ 130, 130, 135, 255 };
    Color scrollbarColor{ 100, 100, 100, 180 };

    float MaxScroll(float height) const {
        float content = rows ? (float)rows->size() * kLineHeight : 0.0f;
        return std::max(0.0f, content - height + kPad * 2.0f);
    }

protected:
    void OnTickSelf(float, Rectangle rect, float) override {
        float maxScroll = MaxScroll(rect.height);
        Vector2 mouse = GetMousePosition();
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f && !WidgetInput::IsMouseBlocked() && CheckCollisionPointRec(mouse, rect)) {
            scroll -= wheel * kLineHeight * 3.0f;
            if (follow) *follow = false;
        }
        scroll = std::clamp(scroll, 0.0f, maxScroll);
        if (follow) {
            if (*follow) scroll = maxScroll;
            else if (maxScroll > 0.0f && scroll >= maxScroll - 0.5f && wheel < 0.0f) *follow = true;
        }
    }

    void OnDrawSelf(Rectangle rect, float) override {
        DrawRectangleRec(rect, background);
        if (!rows || rows->empty()) return;

        ScissorStack::Push(rect);
        int first = std::max(0, (int)(scroll / kLineHeight));
        float y = rect.y + kPad - scroll + (float)first * kLineHeight;
        for (int i = first; i < (int)rows->size() && y < rect.y + rect.height; i++, y += kLineHeight) {
            const LogEntry& entry = (*rows)[i];
            int x = (int)rect.x + 5;
            DrawText(entry.time.c_str(), x, (int)y + 1, 10, timeColor);
            DrawText(entry.message.c_str(), x + 54, (int)y + 1, 10, LogEntryColor(entry.level));
        }
        ScissorStack::Pop();

        float maxScroll = MaxScroll(rect.height);
        if (maxScroll > 0.0f) {
            float visible = rect.height - kPad * 2.0f;
            float content = (float)rows->size() * kLineHeight;
            float barHeight = std::max(16.0f, visible * (visible / content));
            float ratio = scroll / maxScroll;
            float barY = rect.y + kPad + ratio * (visible - barHeight);
            DrawRectangleRec({ rect.x + rect.width - 7.0f, barY, 4.0f, barHeight }, scrollbarColor);
        }
    }
};

}

LogPanel::LogPanel() = default;

LogPanel::~LogPanel() {
    delete chrome;
}

void LogPanel::Initialize() {
    name = "Logs";
}

void LogPanel::AddLog(const std::string& message, LogLevel level) {
    std::time_t now = std::time(nullptr);
    std::string clock = LogFile::MakeClockText(now);
    std::vector<std::string> lines = LogFile::SplitMessage(message);

    std::lock_guard<std::mutex> lock(mutex);
    for (const std::string& line : lines) {
        logs.push_back({ line, level, clock });
        levelCounts[LevelIndex(level)]++;
        totalPushed++;
        writer.Append(LevelToChar(level), clock, line);
        while ((int)logs.size() > maxLogs) {
            levelCounts[LevelIndex(logs.front().level)]--;
            logs.pop_front();
        }
    }
    version++;
}

void LogPanel::Clear() {
    std::lock_guard<std::mutex> lock(mutex);
    logs.clear();
    levelCounts[0] = levelCounts[1] = levelCounts[2] = 0;
    version++;
}

std::vector<LogEntry> LogPanel::GetLogsSince(int fromCount) const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<LogEntry> result;
    int firstIndex = totalPushed - (int)logs.size();
    int offset = fromCount - firstIndex;
    if (offset < 0) offset = 0;
    for (int i = offset; i < (int)logs.size(); i++) result.push_back(logs[i]);
    return result;
}

void LogPanel::EnsureSession() {
    const std::string& root = Project::Get().GetRootPath();
    if (sessionOpened && root == sessionRoot) return;
    sessionOpened = true;
    sessionRoot = root;
    OpenSession(root);
}

void LogPanel::OpenSession(const std::string& root) {
    std::string error;
    std::string keepName;
    fs::path directory;
    {
        std::lock_guard<std::mutex> lock(mutex);
        writer.Close();
        fileError.clear();
        if (!viewedName.empty()) { viewedName.clear(); viewed.clear(); shownDirty = true; }
        if (root.empty()) return;

        if (!writer.Open(fs::path(root), std::time(nullptr), error)) {
            fileError = error;
            return;
        }
        if (!backlogWritten) {
            backlogWritten = true;
            for (const LogEntry& entry : logs) writer.Append(LevelToChar(entry.level), entry.time, entry.message);
        }
        keepName = writer.SessionName();
        directory = writer.Directory();
    }
    LogFile::Prune(directory, keepName, std::time(nullptr), LogFile::kMaxAgeDays, LogFile::kMaxSessions);
}

void LogPanel::LoadViewedSession(const std::string& name) {
    std::vector<LogFile::Entry> loaded;
    bool truncated = false;
    fs::path directory;
    {
        std::lock_guard<std::mutex> lock(mutex);
        directory = writer.Directory();
    }
    if (directory.empty() || !LogFile::ReadSession(directory, name, loaded, truncated)) {
        fileError = "could not read " + name;
        return;
    }
    std::lock_guard<std::mutex> lock(mutex);
    viewed.clear();
    viewed.reserve(loaded.size());
    for (const LogFile::Entry& entry : loaded) viewed.push_back({ entry.message, LevelFromChar(entry.level), entry.time });
    viewedName = name;
    viewedTruncated = truncated;
    follow = false;
    version++;
}

void LogPanel::ReturnToLive() {
    std::lock_guard<std::mutex> lock(mutex);
    viewedName.clear();
    viewed.clear();
    follow = true;
    version++;
}

void LogPanel::RebuildShown() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!shownDirty && shownVersion == version) return;
    shownDirty = false;
    shownVersion = version;
    shown.clear();

    auto consider = [&](const LogEntry& entry) {
        if (!showLevel[LevelIndex(entry.level)]) return;
        if (!searchLower.empty() && Lower(entry.message).find(searchLower) == std::string::npos) return;
        shown.push_back(entry);
    };
    if (!viewedName.empty()) for (const LogEntry& entry : viewed) consider(entry);
    else for (const LogEntry& entry : logs) consider(entry);
}

void LogPanel::CopyShown() {
    std::string text;
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (const LogEntry& entry : shown) text += LogFile::FormatLine(LevelToChar(entry.level), entry.time, entry.message) + "\n";
    }
    SetClipboardText(text.c_str());
    copiedUntil = GetTime() + 1.5;
}

std::string LogPanel::FooterText() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::string text;
    int total = viewedName.empty() ? (int)logs.size() : (int)viewed.size();
    text = std::to_string(shown.size()) + " of " + std::to_string(total) + " lines";
    if (!viewedName.empty()) {
        text += "  |  old session " + viewedName;
        if (viewedTruncated) text += " (start cut off, file larger than 8 MB)";
    } else if (writer.IsOpen()) {
        text += "  |  " + writer.SessionName();
    } else if (!fileError.empty()) {
        text += "  |  not written to disk: " + fileError;
    } else {
        text += "  |  not written to disk (no project open)";
    }
    return text;
}

void LogPanel::EnsureChrome() {
    if (chrome) return;
    const UIPalette& theme = UITheme::Get();

    chrome = new Widget();
    chromeRoot = new WidgetElement();
    chrome->SetRoot(chromeRoot);

    auto makeButton = [&](const std::string& label, std::function<void()> action) {
        auto* button = new LogButtonElement();
        button->label = label;
        button->defaultColor = theme.buttonBg;
        button->hoveredColor = theme.buttonHover;
        button->pressedColor = theme.buttonPressed;
        button->selectedColor = theme.buttonHover;
        button->disabledColor = theme.buttonBg;
        button->textColor = theme.buttonText;
        button->onActivated = std::move(action);
        chromeRoot->AddChild(button);
        return button;
    };

    clearButton = makeButton("Clear", [this]() { if (viewedName.empty()) Clear(); });
    copyButton = makeButton("Copy", [this]() { CopyShown(); });
    sessionButton = makeButton("Session", [this]() { OpenSessionMenu(sessionButton->lastResolvedRect); });
    for (int i = 0; i < 3; i++) {
        levelButtons[i] = makeButton("", [this, i]() { showLevel[i] = !showLevel[i]; shownDirty = true; });
    }

    searchField = new TextElement();
    searchField->editable = true;
    searchField->fontSize = 12.0f;
    searchField->placeholder = "Search...";
    searchField->backgroundColor = theme.fieldBg;
    searchField->borderColor = theme.fieldBorder;
    searchField->editingBorderColor = theme.accent;
    searchField->color = theme.textPrimary;
    searchField->placeholderColor = theme.textSecondary;
    searchField->onTextChanged = [this](const std::string& text) {
        search = text;
        searchLower = Lower(text);
        shownDirty = true;
    };
    chromeRoot->AddChild(searchField);

    followBox = new CheckBoxElement();
    followBox->checked = follow;
    followBox->boxColor = theme.fieldBg;
    followBox->hoveredBoxColor = theme.surfaceHover;
    followBox->borderColor = theme.fieldBorder;
    followBox->checkColor = theme.accent;
    followBox->onToggled = [this](bool checked) { follow = checked; };
    chromeRoot->AddChild(followBox);

    followLabel = new TextElement();
    followLabel->text = "Follow";
    followLabel->fontSize = 12.0f;
    followLabel->color = theme.textSecondary;
    chromeRoot->AddChild(followLabel);

    auto* list = new LogListElement();
    list->rows = &shown;
    list->follow = &follow;
    list->background = theme.panelBg;
    list->timeColor = theme.textSecondary;
    chromeRoot->AddChild(list);
    listElement = list;

    footer = new TextElement();
    footer->fontSize = 11.0f;
    footer->color = theme.textSecondary;
    footer->truncateToRect = true;
    chromeRoot->AddChild(footer);
}

void LogPanel::LayoutChrome() {
    float x = kPad;
    float y = bounds.y + kPad;
    auto place = [&](WidgetElement* element, float width) {
        element->offset = { x, y - bounds.y };
        element->size = { width, kToolbarHeight - 6.0f };
        element->visible = x + width <= bounds.width;
        x += width + 4.0f;
    };
    place(clearButton, 48.0f);
    place(copyButton, 48.0f);
    place(sessionButton, 150.0f);
    for (int i = 0; i < 3; i++) place(levelButtons[i], 74.0f);

    float rowY = kToolbarHeight + kPad - 2.0f;
    float followWidth = 60.0f;
    searchField->offset = { kPad, rowY };
    searchField->size = { std::max(40.0f, bounds.width - kPad * 3.0f - followWidth), kSearchHeight - 4.0f };
    followBox->offset = { bounds.width - followWidth - kPad + 2.0f, rowY + 3.0f };
    followBox->size = { 14.0f, 14.0f };
    followLabel->offset = { bounds.width - followWidth - kPad + 20.0f, rowY + 2.0f };
    followLabel->size = { 40.0f, 16.0f };

    float listTop = kToolbarHeight + kSearchHeight + 2.0f;
    float listHeight = std::max(0.0f, bounds.height - listTop - kFooterHeight);
    listElement->offset = { 1.0f, listTop };
    listElement->size = { std::max(0.0f, bounds.width - 2.0f), listHeight };
    footer->offset = { kPad + 2.0f, listTop + listHeight + 2.0f };
    footer->size = { std::max(0.0f, bounds.width - kPad * 2.0f), kFooterHeight - 2.0f };
}

void LogPanel::UpdateChromeState() {
    const UIPalette& theme = UITheme::Get();
    bool live = viewedName.empty();
    bool copied = GetTime() < copiedUntil;
    static_cast<LogButtonElement*>(clearButton)->label = "Clear";
    clearButton->disabled = !live;
    static_cast<LogButtonElement*>(copyButton)->label = copied ? "Copied" : "Copy";

    std::string sessionLabel = live ? std::string("Current session") : viewedName.substr(0, viewedName.size() - 4) + " (old)";
    static_cast<LogButtonElement*>(sessionButton)->label = sessionLabel;
    static_cast<LogButtonElement*>(sessionButton)->defaultColor = live ? theme.buttonBg : theme.surfaceSelected;

    static const char* const names[3] = { "Info", "Warn", "Error" };
    for (int i = 0; i < 3; i++) {
        int count;
        {
            std::lock_guard<std::mutex> lock(mutex);
            count = 0;
            if (live) count = levelCounts[i];
            else for (const LogEntry& entry : viewed) if (LevelIndex(entry.level) == i) count++;
        }
        auto* button = static_cast<LogButtonElement*>(levelButtons[i]);
        button->label = std::string(names[i]) + " " + std::to_string(count);
        button->defaultColor = showLevel[i] ? theme.surfaceSelected : theme.buttonBg;
        button->textColor = showLevel[i] ? theme.textPrimary : theme.buttonDisabledText;
    }
    followBox->checked = follow;
    footer->text = FooterText();
}

void LogPanel::OpenSessionMenu(Rectangle anchor) {
    menuItems.clear();
    std::vector<ChoiceOption> choices;

    menuItems.push_back({ MenuAction::Live, "" });
    choices.push_back({ "Current session" });

    fs::path directory;
    std::string currentName;
    {
        std::lock_guard<std::mutex> lock(mutex);
        directory = writer.Directory();
        currentName = writer.SessionName();
    }
    int oldCount = 0;
    if (!directory.empty()) {
        for (const LogFile::SessionInfo& session : LogFile::ListSessions(directory)) {
            if (session.name == currentName) continue;
            menuItems.push_back({ MenuAction::Session, session.name });
            choices.push_back({ session.name.substr(0, session.name.size() - 4) + " (old, " + SizeText(session.bytes) + ")" });
            oldCount++;
        }
    }
    menuItems.push_back({ MenuAction::DeleteOld, "" });
    ChoiceOption deleteOption{ "Delete all old logs..." };
    deleteOption.enabled = oldCount > 0;
    choices.push_back(deleteOption);
    menuItems.push_back({ MenuAction::OpenFolder, "" });
    ChoiceOption folderOption{ "Open log folder" };
    folderOption.enabled = !directory.empty();
    choices.push_back(folderOption);

    sessionMenu.Open({ anchor.x, anchor.y + anchor.height }, 240.0f, choices);
}

void LogPanel::PollPopups() {
    int choice = -1;
    if (sessionMenu.IsOpen() && sessionMenu.Render(choice) && choice >= 0 && choice < (int)menuItems.size()) {
        const MenuItem& item = menuItems[choice];
        if (item.action == MenuAction::Live) {
            ReturnToLive();
        } else if (item.action == MenuAction::Session) {
            LoadViewedSession(item.name);
        } else if (item.action == MenuAction::DeleteOld) {
            deleteConfirm.Open("Delete all old log files in this project? The current session is kept.", "Delete");
        } else if (item.action == MenuAction::OpenFolder) {
            fs::path directory;
            {
                std::lock_guard<std::mutex> lock(mutex);
                directory = writer.Directory();
            }
            std::error_code ec;
            if (!directory.empty() && fs::is_directory(directory, ec)) {
                std::string output;
                std::vector<std::string> argv;
#ifdef _WIN32
                argv = { "explorer", directory.make_preferred().string() };
#elif defined(__APPLE__)
                argv = { "open", directory.string() };
#else
                argv = { "xdg-open", directory.string() };
#endif
                SafeProcess::Run(argv, output);
            }
        }
    }

    bool confirmed = false;
    if (deleteConfirm.IsOpen() && deleteConfirm.Render(confirmed) && confirmed) {
        fs::path directory;
        std::string keep;
        {
            std::lock_guard<std::mutex> lock(mutex);
            directory = writer.Directory();
            keep = writer.SessionName();
        }
        if (!directory.empty()) LogFile::DeleteAllExcept(directory, keep);
        if (!viewedName.empty()) ReturnToLive();
    }
}

void LogPanel::Render() {
    EnsureSession();
    EnsureChrome();
    PollPopups();
    RebuildShown();

    LayoutChrome();
    UpdateChromeState();

    const UIPalette& theme = UITheme::Get();
    DrawRectangleRec(bounds, theme.panelBg);

    float dt = GetFrameTime();
    chrome->Tick(dt, bounds, 1.0f);
    WidgetInput::ProcessInput(chrome, WidgetInput::kEditorDevice, dt, GetMousePosition(), 1.0f);
    chrome->Draw(bounds, 1.0f);

    DrawRectangleLinesEx(bounds, 1, theme.panelBorder);
}
