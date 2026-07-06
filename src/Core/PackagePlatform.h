#pragma once
#include <string>
#include <vector>

enum class PackagePlatform {
    Windows,
    Linux,
    macOS,
    Web,
    Android,
};

struct PackagePlatformInfo {
    PackagePlatform platform;
    std::string name;
    std::string displayName;
    bool available = false;
    std::string unavailableReason;
};

std::string PackagePlatformName(PackagePlatform platform);
PackagePlatform PackagePlatformFromName(const std::string& name);

std::vector<PackagePlatformInfo> GetAvailablePackagePlatforms();
