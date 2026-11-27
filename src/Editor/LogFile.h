#pragma once
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace LogFile {

constexpr size_t kMaxLineBytes = 4000;
constexpr long long kMaxSessionBytes = 20LL * 1024 * 1024;
constexpr long long kMaxReadBytes = 8LL * 1024 * 1024;
constexpr int kMaxSessions = 30;
constexpr int kMaxAgeDays = 14;

struct Entry {
    char level = 'I';
    std::string time;
    std::string message;
};

struct SessionInfo {
    std::string name;
    long long bytes = 0;
};

std::string MakeSessionName(std::time_t when, int suffix = 0);
std::string MakeClockText(std::time_t when);
bool IsSessionFileName(const std::string& name);
bool ParseSessionTime(const std::string& name, std::time_t& out);

std::vector<std::string> SplitMessage(const std::string& message);
std::string FormatLine(char level, const std::string& time, const std::string& message);
bool ParseLine(const std::string& line, Entry& out);

std::filesystem::path LogsDirectory(const std::filesystem::path& projectRoot);

class Writer {
public:
    ~Writer();

    bool Open(const std::filesystem::path& projectRoot, std::time_t now, std::string& error);
    void Close();
    bool IsOpen() const { return open; }
    void Append(char level, const std::string& time, const std::string& message);

    const std::filesystem::path& Directory() const { return directory; }
    const std::string& SessionName() const { return sessionName; }
    std::filesystem::path FilePath() const { return directory / sessionName; }
    long long BytesWritten() const { return bytes; }

private:
    std::ofstream stream;
    std::filesystem::path directory;
    std::string sessionName;
    long long bytes = 0;
    bool open = false;
    bool limitReported = false;
};

std::vector<SessionInfo> ListSessions(const std::filesystem::path& directory);
int Prune(const std::filesystem::path& directory, const std::string& keepName, std::time_t now, int maxAgeDays, int maxSessions);
int DeleteAllExcept(const std::filesystem::path& directory, const std::string& keepName);
bool ReadSession(const std::filesystem::path& directory, const std::string& name, std::vector<Entry>& out, bool& truncated);

}
