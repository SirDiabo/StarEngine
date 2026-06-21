#include "Project.h"
#include "raylib.h"

Project& Project::Get() {
    static Project instance;
    return instance;
}

bool Project::Load(const std::string& path) {
    rootPath = path;
    settings.name = GetFileNameWithoutExt(path.c_str());
    return true;
}

void Project::Save() {
}