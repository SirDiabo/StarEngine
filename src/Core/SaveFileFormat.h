#pragma once
#include <string>
#include <unordered_map>

namespace SaveFileFormat {
    std::unordered_map<std::string, std::string> ReadEntries(const std::string& saveFileName);

    bool WriteEntries(const std::string& saveFileName, const std::unordered_map<std::string, std::string>& updates);
    std::unordered_map<std::string, std::string> ReadEntriesInSlot(const std::string& slot, const std::string& saveFileName);
    bool WriteEntriesInSlot(const std::string& slot, const std::string& saveFileName, const std::unordered_map<std::string, std::string>& updates);
    void DiscardSlotCache(const std::string& slot);

    void FlushAll();
    void InvalidateCache();
}
