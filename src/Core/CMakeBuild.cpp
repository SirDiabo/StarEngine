#include "CMakeBuild.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace CMakeBuild {

bool CacheSourceMismatch(const std::string& buildDir, const std::string& sourceDir) {
    fs::path cachePath = fs::path(buildDir) / "CMakeCache.txt";
    if (!fs::exists(cachePath)) return false;

    std::ifstream in(cachePath);
    std::string line;
    std::string cachedSource;
    const std::string key = "CMAKE_HOME_DIRECTORY:INTERNAL=";
    while (std::getline(in, line)) {
        if (line.rfind(key, 0) == 0) {
            cachedSource = line.substr(key.size());
            break;
        }
    }
    if (cachedSource.empty()) return false;

    std::error_code ec;
    fs::path cached = fs::weakly_canonical(fs::path(cachedSource), ec);
    fs::path current = fs::weakly_canonical(fs::path(sourceDir), ec);
    return cached != current;
}

}
