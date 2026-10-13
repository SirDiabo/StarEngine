#pragma once
#include <filesystem>
#include <functional>
#include <string>

namespace ContentUndo {

void RecordMove(const std::filesystem::path& from, const std::filesystem::path& to, const std::string& label,
                std::function<void(const std::filesystem::path& currentPath)> afterMove = {});
void RecordFileEdit(const std::string& path, const std::string& before, const std::string& after,
                    const std::string& label, std::function<void()> afterRestore = {});
bool ReadFileBytes(const std::string& path, std::string& out);

}
