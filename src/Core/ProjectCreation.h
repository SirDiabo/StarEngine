#pragma once
#include <filesystem>
#include <string>

namespace ProjectCreation {

bool IsValidProjectName(const std::string& name);

bool CreateFolders(const std::filesystem::path& projectsDir, const std::string& name,
                   std::filesystem::path& outProjectFile, std::string& error);

}
