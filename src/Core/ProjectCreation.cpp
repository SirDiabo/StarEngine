#include "ProjectCreation.h"
#include "PathSafety.h"
#include <algorithm>
#include <cctype>
#include <fstream>

namespace fs = std::filesystem;

namespace {

bool IsReservedDeviceName(const std::string& name) {
    std::string stem = name.substr(0, name.find('.'));
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return (char)std::toupper(c); });
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL") return true;
    if (stem.size() == 4 && (stem.compare(0, 3, "COM") == 0 || stem.compare(0, 3, "LPT") == 0)) {
        return stem[3] >= '1' && stem[3] <= '9';
    }
    return false;
}

const char* const kGitIgnoreText =
    "# Git Ignore File for StarEngine Projects\n"
    "\n"
    "# Save Files\n"
    "Saved/*\n"
    "\n"
    "# Package Output\n"
    "Packaged/*\n";

}

namespace ProjectCreation {

bool IsValidProjectName(const std::string& name) {
    if (!PathSafety::IsPlainFileName(name) || name.size() > 100) return false;
    if (name.front() == ' ' || name.back() == ' ' || name.back() == '.') return false;
    if (name.find_first_of("<>\"|?*") != std::string::npos) return false;
    return !IsReservedDeviceName(name);
}

bool CreateFolders(const fs::path& projectsDir, const std::string& name, fs::path& outProjectFile, std::string& error) {
    if (!IsValidProjectName(name)) {
        error = "A project name cannot be empty, contain \\ / : < > \" | ? * or control characters, start or end with a space, end with a dot, or be a reserved system name.";
        return false;
    }

    std::error_code ec;
    const fs::path projectDir = projectsDir / name;
    const fs::path projectFile = projectDir / (name + ".starproject");
    if (fs::exists(projectFile, ec)) {
        error = "A project with this name already exists.";
        return false;
    }

    for (const char* sub : { "", "Content", "Config", "Saved" }) {
        const fs::path dir = projectDir / sub;
        fs::create_directories(dir, ec);
        if (ec) {
            error = "Could not create " + dir.string() + ": " + ec.message();
            return false;
        }
    }

    {
        std::ofstream out(projectFile);
        if (!out) {
            error = "Could not create " + projectFile.string();
            return false;
        }
    }

    const fs::path gitIgnore = projectDir / ".gitignore";
    if (!fs::exists(gitIgnore, ec)) {
        std::ofstream out(gitIgnore);
        out << kGitIgnoreText;
    }

    outProjectFile = projectFile;
    return true;
}

}
