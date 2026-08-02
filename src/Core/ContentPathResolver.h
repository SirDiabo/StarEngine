#pragma once
#include <string>

namespace ContentPathResolver {
    void SetContentRoot(const std::string& root);
    std::string Resolve(const std::string& path);
    std::string MakeRelative(const std::string& path);
}