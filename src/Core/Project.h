#pragma once
#include <string>
#include <vector>

enum class GameRenderer {
    Hardware,
    Software,
};

std::string GameRendererName(GameRenderer renderer);
GameRenderer GameRendererFromName(const std::string& name);

struct ProjectSettings {
    std::string name;
    int targetWidth = 640;
    int targetHeight = 480;
    std::string defaultMapPath;
    GameRenderer renderer = GameRenderer::Hardware;
    std::vector<std::string> disabledModules;
};

class Project {
public:
    static Project& Get();
    static std::string GetProjectsDirectory();

    bool Load(const std::string& projectFilePath);
    void Save();

    ProjectSettings& GetSettings() { return settings; }
    const ProjectSettings& GetSettings() const { return settings; }
    const std::string& GetRootPath() const { return rootPath; }
    const std::string& GetProjectFilePath() const { return projectFilePath; }

    std::string GetModulesDirectory() const;

private:
    void WriteActiveProjectPointer() const;

private:
    ProjectSettings settings;
    std::string rootPath;
    std::string projectFilePath;
};