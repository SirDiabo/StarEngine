#pragma once
#include <string>

namespace BuildCacheGuard {

struct Preparation {
    bool ok = false;
    bool wiped = false;
    std::string path;
    std::string error;
};

const char* MarkerFileName();

std::string ExpectedMarker(const std::string& canonicalCacheDir, const std::string& machineToken);

Preparation Prepare(const std::string& projectRoot, const std::string& cacheName, const std::string& machineToken);

bool Reset(const std::string& preparedCachePath, const std::string& machineToken, std::string& error);

}
