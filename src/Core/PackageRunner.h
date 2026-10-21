#pragma once
#include "PackagePlatform.h"
#include "PackageCipher.h"
#include "PackageCompression.h"
#include "Project.h"
#include <string>
#include <functional>

enum class PackageFormat {
    SingleFile,
    LooseFolder,
};

std::string PackageFormatName(PackageFormat format);
PackageFormat PackageFormatFromName(const std::string& name);

struct PackageOptions {
    std::string projectRoot;
    std::string outputDir;
    PackagePlatform platform = PackagePlatform::Linux;
    GameRenderer renderer = GameRenderer::Hardware;
    std::string cipher = CookCiphers::None;
    std::string compression = PackCompressions::None;
    bool integrity = true;
    PackageFormat format = PackageFormat::SingleFile;
};

struct PackageResult {
    bool success = false;
    std::string log;
    std::string error;
    std::string outputExecutable;
};

namespace PackageRunner {
    PackageResult Package(const PackageOptions& options,
                           const std::function<void(const std::string&)>& onOutput = nullptr);
}
