#pragma once
#include <string>

namespace SavePaths {
    void SetPackagedGameName(const std::string& name);

    std::string GetSaveDirectory();
    std::string GetSaveFilePath(const std::string& saveFileName);
}
