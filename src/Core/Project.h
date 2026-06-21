#pragma once
#include <string>

struct Vec3 {
    float x, y, z;
    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

struct ProjectSettings {
    std::string name;
    int targetWidth = 640;
    int targetHeight = 480;
    std::string startupModule;
};

class Project {
public:
    static Project& Get();

    bool Load(const std::string& path);
    void Save();

    const ProjectSettings& GetSettings() const { return settings; }
    const std::string& GetRootPath() const { return rootPath; }

private:
    ProjectSettings settings;
    std::string rootPath;
};