#include "Project.h"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

Project& Project::Get() {
    static Project instance;
    return instance;
}

std::string Project::GetProjectsDirectory() {
    const char* home = getenv("USERPROFILE");
    if (!home) home = getenv("HOME");

    fs::path docsPath = home ? home : ".";
    docsPath /= "Documents";
    docsPath /= "StarProjects";
    return docsPath.string();
}

bool Project::Load(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    projectFilePath = path;
    rootPath = fs::path(path).parent_path().string();
    settings = ProjectSettings();
    settings.name = GetFileNameWithoutExt(path.c_str());

    std::string line;
    while (std::getline(in, line)) {
        size_t separator = line.find('=');
        if (separator == std::string::npos) continue;

        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        if (!value.empty() && value.back() == '\r') value.pop_back();

        if (key == "name") settings.name = value;
        else if (key == "targetWidth") settings.targetWidth = std::stoi(value);
        else if (key == "targetHeight") settings.targetHeight = std::stoi(value);
        else if (key == "startupModule") settings.startupModule = value;
    }

    return true;
}

void Project::Save() {
    if (projectFilePath.empty()) return;

    std::ofstream out(projectFilePath);
    if (!out.is_open()) return;

    out << "name=" << settings.name << "\n";
    out << "targetWidth=" << settings.targetWidth << "\n";
    out << "targetHeight=" << settings.targetHeight << "\n";
    out << "startupModule=" << settings.startupModule << "\n";
}
