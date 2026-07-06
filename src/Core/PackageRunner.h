#pragma once
#include "PackagePlatform.h"
#include "PackageCipher.h"
#include "Project.h"
#include <string>

struct PackageOptions {
    std::string projectRoot;
    std::string outputDir;
    PackagePlatform platform = PackagePlatform::Linux;
    GameRenderer renderer = GameRenderer::Hardware;
    CookCipher cipher = CookCipher::None;
};

struct PackageResult {
    bool success = false;
    std::string log;
    std::string error;
    std::string outputExecutable;
};

namespace PackageRunner {
    PackageResult Package(const PackageOptions& options);
}
