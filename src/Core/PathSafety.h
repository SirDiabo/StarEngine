#pragma once
#include <filesystem>
#include <string>

namespace PathSafety {

bool IsPlainFileName(const std::string& name);

bool IsInside(const std::filesystem::path& root, const std::filesystem::path& candidate);

bool IsImageExtension(const std::filesystem::path& path);
bool IsMtlExtension(const std::filesystem::path& path);
bool IsMediaSourceExtension(const std::filesystem::path& path);

std::filesystem::path ResolveMtlReference(const std::filesystem::path& baseDir, const std::string& reference);
std::filesystem::path ResolveTextureReference(const std::filesystem::path& baseDir, const std::string& reference);

}
