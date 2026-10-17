#pragma once
#include <string>

namespace SavePaths {
    void SetPackagedGameName(const std::string& name);

    std::string GetSaveDirectory();
    std::string GetSaveFilePath(const std::string& saveFileName);

    std::string NormalizeSlotName(const std::string& raw);
    void SetCurrentSlot(const std::string& slot);
    const std::string& GetCurrentSlot();
    std::string GetSlotsRoot();
    std::string GetSlotDirectory(const std::string& slot);
    std::string GetSaveFilePathInSlot(const std::string& slot, const std::string& saveFileName);
}
