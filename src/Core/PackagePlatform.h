#pragma once
#include <atomic>
#include <memory>
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

struct PackagePlatformProbe {
    std::atomic<bool> ready{false};
    std::vector<PackagePlatformInfo> platforms;
};

std::string PackagePlatformName(PackagePlatform platform);
PackagePlatform PackagePlatformFromName(const std::string& name);

std::vector<PackagePlatformInfo> GetAvailablePackagePlatforms();
std::vector<PackagePlatformInfo> GetPendingPackagePlatforms();
std::shared_ptr<PackagePlatformProbe> StartPackagePlatformProbe();
