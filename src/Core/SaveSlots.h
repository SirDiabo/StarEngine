#pragma once
#include <string>
#include <unordered_map>
#include <vector>

struct SlotInfo {
    std::string name;
    bool exists = false;
    long long createdUnix = 0;
    long long modifiedUnix = 0;
    double playtimeSeconds = 0.0;
    std::string levelName;
    std::unordered_map<std::string, std::string> custom;
};

namespace SaveSlots {
    bool IsMetadataFile(const std::string& saveFileName);

    SlotInfo GetInfo(const std::string& slot);
    bool Exists(const std::string& slot);
    std::vector<SlotInfo> List();

    void SetCurrent(const std::string& slot);
    bool Delete(const std::string& slot);
    bool Copy(const std::string& fromSlot, const std::string& toSlot);

    void SetCustomField(const std::string& key, const std::string& value);
    std::string GetCustomField(const std::string& slot, const std::string& key);

    void OnPlayBegin();
    void NotifySaved();

    std::string FormatTimestamp(long long unixSeconds);
}
