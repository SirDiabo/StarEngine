#pragma once
#include <string>

struct ProjectSettings {
    std::string name;
    int targetWidth = 640;
    int targetHeight = 480;
    std::string startupModule;
};

class Project {
public:
    static Project& Get();
    static std::string GetProjectsDirectory();

    bool Load(const std::string& projectFilePath);
    void Save();

    const ProjectSettings& GetSettings() const { return settings; }
    const std::string& GetRootPath() const { return rootPath; }

private:
    ProjectSettings settings;
    std::string rootPath;
    std::string projectFilePath;
};
