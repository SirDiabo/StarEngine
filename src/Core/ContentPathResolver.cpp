#include "ContentPathResolver.h"
#include <filesystem>

namespace fs = std::filesystem;

static std::string contentRoot;

void ContentPathResolver::SetContentRoot(const std::string& root) {
    contentRoot = root;
}

std::string ContentPathResolver::Resolve(const std::string& path) {
    if (path.empty() || contentRoot.empty()) return path;
    if (fs::path(path).is_absolute()) return path;
    return (fs::path(contentRoot) / path).string();
}

std::string ContentPathResolver::MakeRelative(const std::string& path) {
    if (path.empty() || contentRoot.empty()) return path;
    fs::path p(path);
    if (!p.is_absolute()) return path;

    std::error_code ec;
    fs::path rel = fs::relative(p, contentRoot, ec);
    if (ec) return path;
    return rel.generic_string();
}