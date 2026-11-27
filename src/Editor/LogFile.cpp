#include "LogFile.h"
#include "Core/PathSafety.h"
#include <algorithm>
#include <cstdio>
#include <iterator>

namespace fs = std::filesystem;

namespace {

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

bool IsRegularNonLink(const fs::path& path) {
    std::error_code ec;
    fs::file_status status = fs::symlink_status(path, ec);
    if (ec) return false;
    return fs::is_regular_file(status);
}

bool IsRealDirectory(const fs::path& path) {
    std::error_code ec;
    fs::file_status status = fs::symlink_status(path, ec);
    if (ec) return false;
    return fs::is_directory(status) && !fs::is_symlink(status);
}

std::tm LocalTime(std::time_t when) {
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &when);
#else
    localtime_r(&when, &result);
#endif
    return result;
}

int ParseNumber(const std::string& text, size_t start, size_t count) {
    int value = 0;
    for (size_t i = 0; i < count; i++) value = value * 10 + (text[start + i] - '0');
    return value;
}

}

namespace LogFile {

std::string MakeSessionName(std::time_t when, int suffix) {
    std::tm t = LocalTime(when);
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d_%02d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec);
    std::string name = buffer;
    if (suffix > 0) name += "_" + std::to_string(suffix);
    return name + ".log";
}

std::string MakeClockText(std::time_t when) {
    std::tm t = LocalTime(when);
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    return buffer;
}

bool IsSessionFileName(const std::string& name) {
    static const char pattern[] = "dddd-dd-dd_dd-dd-dd";
    const size_t stampLength = sizeof(pattern) - 1;
    if (name.size() < stampLength + 4) return false;
    for (size_t i = 0; i < stampLength; i++) {
        if (pattern[i] == 'd' ? !IsDigit(name[i]) : name[i] != pattern[i]) return false;
    }
    size_t pos = stampLength;
    if (name[pos] == '_') {
        pos++;
        size_t digitsStart = pos;
        while (pos < name.size() && IsDigit(name[pos])) pos++;
        if (pos == digitsStart || pos - digitsStart > 6) return false;
    }
    return name.compare(pos, std::string::npos, ".log") == 0;
}

bool ParseSessionTime(const std::string& name, std::time_t& out) {
    if (!IsSessionFileName(name)) return false;
    std::tm t{};
    t.tm_year = ParseNumber(name, 0, 4) - 1900;
    t.tm_mon = ParseNumber(name, 5, 2) - 1;
    t.tm_mday = ParseNumber(name, 8, 2);
    t.tm_hour = ParseNumber(name, 11, 2);
    t.tm_min = ParseNumber(name, 14, 2);
    t.tm_sec = ParseNumber(name, 17, 2);
    t.tm_isdst = -1;
    std::time_t value = std::mktime(&t);
    if (value == (std::time_t)-1) return false;
    out = value;
    return true;
}

std::vector<std::string> SplitMessage(const std::string& message) {
    std::vector<std::string> lines;
    std::string current;
    auto flush = [&]() {
        while (!current.empty() && current.back() == ' ') current.pop_back();
        if (current.size() > kMaxLineBytes) {
            size_t cut = kMaxLineBytes;
            while (cut > 0 && (static_cast<unsigned char>(current[cut]) & 0xC0) == 0x80) cut--;
            current.resize(cut);
            current += "...";
        }
        lines.push_back(current);
        current.clear();
    };
    for (char c : message) {
        if (c == '\n') flush();
        else if (c == '\r') continue;
        else if (c == '\t') current += "    ";
        else if (static_cast<unsigned char>(c) < 0x20) continue;
        else current += c;
    }
    flush();
    while (lines.size() > 1 && lines.back().empty()) lines.pop_back();
    return lines;
}

std::string FormatLine(char level, const std::string& time, const std::string& message) {
    std::string stamp = time.size() == 8 ? time : std::string("--:--:--");
    char safeLevel = (level == 'W' || level == 'E') ? level : 'I';
    return stamp + " " + safeLevel + " " + message;
}

bool ParseLine(const std::string& line, Entry& out) {
    if (line.size() < 11) return false;
    if (line[2] != ':' || line[5] != ':' || line[8] != ' ' || line[10] != ' ') return false;
    char level = line[9];
    if (level != 'I' && level != 'W' && level != 'E') return false;
    out.time = line.substr(0, 8);
    out.level = level;
    out.message = line.substr(11);
    return true;
}

fs::path LogsDirectory(const fs::path& projectRoot) {
    return projectRoot / "Saved" / "logs";
}

Writer::~Writer() {
    Close();
}

bool Writer::Open(const fs::path& projectRoot, std::time_t now, std::string& error) {
    Close();
    error.clear();
    if (projectRoot.empty()) { error = "no project folder"; return false; }

    std::error_code ec;
    fs::path saved = projectRoot / "Saved";
    fs::path logs = LogsDirectory(projectRoot);

    if (!IsRealDirectory(projectRoot)) { error = "project folder is not a directory"; return false; }
    for (const fs::path& dir : { saved, logs }) {
        fs::file_status status = fs::symlink_status(dir, ec);
        if (!ec && fs::exists(status) && (fs::is_symlink(status) || !fs::is_directory(status))) {
            error = "refusing to write logs through " + dir.string();
            return false;
        }
        ec.clear();
    }
    fs::create_directories(logs, ec);
    if (ec || !IsRealDirectory(logs) || !PathSafety::IsInside(projectRoot, logs)) {
        error = "could not create log folder " + logs.string();
        return false;
    }

    std::string name;
    for (int suffix = 0; suffix < 1000; suffix++) {
        std::string candidate = MakeSessionName(now, suffix);
        std::error_code statusEc;
        fs::file_status status = fs::symlink_status(logs / candidate, statusEc);
        if (fs::status_known(status) && !fs::exists(status)) { name = candidate; break; }
    }
    if (name.empty()) { error = "could not pick a log file name"; return false; }

    stream.open(logs / name, std::ios::out | std::ios::binary);
    if (!stream.is_open()) { error = "could not open " + (logs / name).string(); return false; }

    directory = logs;
    sessionName = name;
    bytes = 0;
    limitReported = false;
    open = true;
    return true;
}

void Writer::Close() {
    if (stream.is_open()) stream.close();
    open = false;
}

void Writer::Append(char level, const std::string& time, const std::string& message) {
    if (!open) return;
    if (bytes >= kMaxSessionBytes) {
        if (!limitReported) {
            limitReported = true;
            std::string note = FormatLine('W', time, "[log file size limit reached, further lines are not written to disk]") + "\n";
            stream.write(note.data(), (std::streamsize)note.size());
            stream.flush();
        }
        return;
    }
    std::string line = FormatLine(level, time, message) + "\n";
    stream.write(line.data(), (std::streamsize)line.size());
    stream.flush();
    if (!stream) { open = false; return; }
    bytes += (long long)line.size();
}

std::vector<SessionInfo> ListSessions(const fs::path& directory) {
    std::vector<SessionInfo> result;
    std::error_code ec;
    if (!IsRealDirectory(directory)) return result;
    for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        std::string name = it->path().filename().string();
        if (!IsSessionFileName(name)) continue;
        if (!IsRegularNonLink(it->path())) continue;
        std::error_code sizeEc;
        auto size = fs::file_size(it->path(), sizeEc);
        SessionInfo info;
        info.name = name;
        info.bytes = sizeEc ? 0 : (long long)size;
        result.push_back(info);
    }
    std::sort(result.begin(), result.end(), [](const SessionInfo& a, const SessionInfo& b) { return a.name > b.name; });
    return result;
}

int Prune(const fs::path& directory, const std::string& keepName, std::time_t now, int maxAgeDays, int maxSessions) {
    std::vector<SessionInfo> sessions = ListSessions(directory);
    int removed = 0;
    int kept = 0;
    const double maxAgeSeconds = (double)maxAgeDays * 86400.0;
    for (const SessionInfo& session : sessions) {
        if (session.name == keepName) { kept++; continue; }
        std::time_t stamp = 0;
        bool tooOld = ParseSessionTime(session.name, stamp) && std::difftime(now, stamp) > maxAgeSeconds;
        bool tooMany = kept >= maxSessions;
        if (tooOld || tooMany) {
            std::error_code ec;
            if (fs::remove(directory / session.name, ec) && !ec) removed++;
        } else {
            kept++;
        }
    }
    return removed;
}

int DeleteAllExcept(const fs::path& directory, const std::string& keepName) {
    int removed = 0;
    for (const SessionInfo& session : ListSessions(directory)) {
        if (session.name == keepName) continue;
        std::error_code ec;
        if (fs::remove(directory / session.name, ec) && !ec) removed++;
    }
    return removed;
}

bool ReadSession(const fs::path& directory, const std::string& name, std::vector<Entry>& out, bool& truncated) {
    out.clear();
    truncated = false;
    if (!IsSessionFileName(name)) return false;
    fs::path path = directory / name;
    if (!IsRegularNonLink(path)) return false;

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();
    if (size < 0) return false;
    std::streamoff start = 0;
    if (size > kMaxReadBytes) { start = size - kMaxReadBytes; truncated = true; }
    in.seekg(start, std::ios::beg);

    std::string data((size_t)(size - start), '\0');
    in.read(&data[0], (std::streamsize)data.size());
    data.resize((size_t)in.gcount());

    size_t pos = 0;
    if (truncated) {
        size_t firstBreak = data.find('\n');
        pos = firstBreak == std::string::npos ? data.size() : firstBreak + 1;
    }
    while (pos < data.size()) {
        size_t end = data.find('\n', pos);
        if (end == std::string::npos) end = data.size();
        std::string line = data.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        Entry entry;
        if (ParseLine(line, entry)) out.push_back(entry);
        else if (!line.empty()) { entry.level = 'I'; entry.time = "--:--:--"; entry.message = line; out.push_back(entry); }
        pos = end + 1;
    }
    return true;
}

}
