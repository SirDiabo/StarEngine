#include "SaveSlots.h"
#include "SavePaths.h"
#include "SaveFileFormat.h"
#include "Runtime/World.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

namespace {
    const char* const kMetadataFile = "@slot";
    const char* const kCustomPrefix = "c.";

    using Clock = std::chrono::steady_clock;

    std::string sessionSlot;
    bool sessionLoaded = false;
    double sessionPlaytime = 0.0;
    Clock::time_point lastMark;
    bool markValid = false;
    Clock::time_point lastNotify;
    bool notifyValid = false;
    std::string lastNotifySlot;

    long long NowUnix() {
        return (long long)std::time(nullptr);
    }

    long long ParseInt64(const std::string& text) {
        return text.empty() ? 0 : std::strtoll(text.c_str(), nullptr, 10);
    }

    double ParseDouble(const std::string& text) {
        return text.empty() ? 0.0 : std::strtod(text.c_str(), nullptr);
    }

    std::string FormatDouble(double value) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.3f", value);
        return buf;
    }

    SlotInfo ReadInfo(const std::string& slot) {
        SlotInfo info;
        info.name = SavePaths::NormalizeSlotName(slot);
        auto entries = SaveFileFormat::ReadEntriesInSlot(info.name, kMetadataFile);
        if (entries.empty()) return info;

        info.exists = true;
        auto find = [&](const char* key) -> const std::string* {
            auto it = entries.find(key);
            return it == entries.end() ? nullptr : &it->second;
        };
        if (const std::string* v = find("created")) info.createdUnix = ParseInt64(*v);
        if (const std::string* v = find("modified")) info.modifiedUnix = ParseInt64(*v);
        if (const std::string* v = find("level")) info.levelName = *v;
        if (const std::string* v = find("playtime")) info.playtimeSeconds = ParseDouble(*v);

        const std::string prefix = kCustomPrefix;
        for (const auto& kv : entries) {
            if (kv.first.compare(0, prefix.size(), prefix) == 0) info.custom[kv.first.substr(prefix.size())] = kv.second;
        }
        return info;
    }

    bool HasSaveFiles(const fs::path& dir, bool recurse) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) return false;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            if (it->is_regular_file(ec) && it->path().extension() == ".starsav") return true;
        }
        (void)recurse;
        return false;
    }

    void EnsureSessionLoaded(const std::string& slot) {
        if (sessionLoaded && sessionSlot == slot) return;
        sessionSlot = slot;
        sessionPlaytime = ReadInfo(slot).playtimeSeconds;
        sessionLoaded = true;
        if (!markValid) {
            lastMark = Clock::now();
            markValid = true;
        }
    }

    void AccumulatePlaytime() {
        Clock::time_point now = Clock::now();
        if (markValid) sessionPlaytime += std::chrono::duration<double>(now - lastMark).count();
        lastMark = now;
        markValid = true;
    }

    std::vector<fs::path> CollectSaveFiles(const std::string& slot) {
        std::vector<fs::path> files;
        std::error_code ec;
        fs::path dir = SavePaths::GetSlotDirectory(slot);
        if (!fs::is_directory(dir, ec)) return files;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            if (it->is_regular_file(ec) && it->path().extension() == ".starsav") files.push_back(it->path());
        }
        return files;
    }
}

bool SaveSlots::IsMetadataFile(const std::string& saveFileName) {
    return saveFileName == kMetadataFile;
}

SlotInfo SaveSlots::GetInfo(const std::string& slot) {
    return ReadInfo(slot);
}

bool SaveSlots::Exists(const std::string& slot) {
    return ReadInfo(slot).exists;
}

std::vector<SlotInfo> SaveSlots::List() {
    SaveFileFormat::FlushAll();

    std::vector<SlotInfo> slots;
    SlotInfo defaultSlot = ReadInfo("");
    if (defaultSlot.exists) slots.push_back(std::move(defaultSlot));

    std::error_code ec;
    fs::path root = SavePaths::GetSlotsRoot();
    if (fs::is_directory(root, ec)) {
        for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_directory(ec)) continue;
            std::string name = it->path().filename().string();
            if (SavePaths::NormalizeSlotName(name) != name) continue;
            SlotInfo info = ReadInfo(name);
            if (!info.exists) {
                if (!HasSaveFiles(it->path(), false)) continue;
                info.exists = true;
            }
            slots.push_back(std::move(info));
        }
    }

    std::sort(slots.begin(), slots.end(), [](const SlotInfo& a, const SlotInfo& b) {
        if (a.modifiedUnix != b.modifiedUnix) return a.modifiedUnix > b.modifiedUnix;
        return a.name < b.name;
    });
    return slots;
}

void SaveSlots::SetCurrent(const std::string& slot) {
    SavePaths::SetCurrentSlot(slot);
    sessionSlot = SavePaths::GetCurrentSlot();
    sessionPlaytime = ReadInfo(sessionSlot).playtimeSeconds;
    sessionLoaded = true;
    lastMark = Clock::now();
    markValid = true;
    notifyValid = false;
}

bool SaveSlots::Delete(const std::string& slot) {
    std::string name = SavePaths::NormalizeSlotName(slot);
    SaveFileFormat::DiscardSlotCache(name);

    std::error_code ec;
    bool removed = false;
    if (name.empty()) {
        for (const fs::path& file : CollectSaveFiles("")) removed |= fs::remove(file, ec);
    } else {
        fs::path dir = SavePaths::GetSlotDirectory(name);
        if (fs::is_directory(dir, ec)) removed = fs::remove_all(dir, ec) > 0;
    }

    if (sessionSlot == name) {
        sessionLoaded = false;
        notifyValid = false;
    }
    return removed;
}

bool SaveSlots::Copy(const std::string& fromSlot, const std::string& toSlot) {
    std::string from = SavePaths::NormalizeSlotName(fromSlot);
    std::string to = SavePaths::NormalizeSlotName(toSlot);
    if (from == to) return false;

    SaveFileFormat::FlushAll();
    std::vector<fs::path> files = CollectSaveFiles(from);
    if (files.empty()) return false;

    SaveFileFormat::DiscardSlotCache(to);

    std::error_code ec;
    fs::path destination = SavePaths::GetSlotDirectory(to);
    fs::create_directories(destination, ec);
    if (ec) return false;

    bool allCopied = true;
    for (const fs::path& file : files) {
        fs::copy_file(file, destination / file.filename(), fs::copy_options::overwrite_existing, ec);
        if (ec) { allCopied = false; ec.clear(); }
    }

    long long now = NowUnix();
    SaveFileFormat::WriteEntriesInSlot(to, kMetadataFile, { {"created", std::to_string(now)}, {"modified", std::to_string(now)} });
    SaveFileFormat::FlushAll();
    return allCopied;
}

void SaveSlots::SetCustomField(const std::string& key, const std::string& value) {
    if (key.empty()) return;
    const std::string& slot = SavePaths::GetCurrentSlot();
    std::unordered_map<std::string, std::string> updates = { {std::string(kCustomPrefix) + key, value} };
    if (!ReadInfo(slot).exists) {
        long long now = NowUnix();
        updates["created"] = std::to_string(now);
        updates["modified"] = std::to_string(now);
    }
    SaveFileFormat::WriteEntriesInSlot(slot, kMetadataFile, updates);
}

std::string SaveSlots::GetCustomField(const std::string& slot, const std::string& key) {
    SlotInfo info = ReadInfo(slot);
    auto it = info.custom.find(key);
    return it == info.custom.end() ? std::string() : it->second;
}

void SaveSlots::OnPlayBegin() {
    SavePaths::SetCurrentSlot("");
    sessionLoaded = false;
    sessionSlot.clear();
    lastMark = Clock::now();
    markValid = true;
    notifyValid = false;
}

void SaveSlots::NotifySaved() {
    const std::string& slot = SavePaths::GetCurrentSlot();
    Clock::time_point now = Clock::now();
    if (notifyValid && lastNotifySlot == slot && now - lastNotify < std::chrono::milliseconds(500)) return;
    lastNotify = now;
    lastNotifySlot = slot;
    notifyValid = true;

    EnsureSessionLoaded(slot);
    AccumulatePlaytime();

    long long nowUnix = NowUnix();
    std::unordered_map<std::string, std::string> updates;
    updates["modified"] = std::to_string(nowUnix);
    updates["level"] = World::Get().GetCurrentLevelName();
    updates["playtime"] = FormatDouble(sessionPlaytime);

    auto existing = SaveFileFormat::ReadEntriesInSlot(slot, kMetadataFile);
    if (existing.find("created") == existing.end()) updates["created"] = std::to_string(nowUnix);

    SaveFileFormat::WriteEntriesInSlot(slot, kMetadataFile, updates);
}

std::string SaveSlots::FormatTimestamp(long long unixSeconds) {
    if (unixSeconds <= 0) return "";
    std::time_t t = (std::time_t)unixSeconds;
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);
    return buf;
}
