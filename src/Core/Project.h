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
    std::string editorDefaultMapPath;
    std::string gameDefaultMapPath;
    GameRenderer renderer = GameRenderer::Hardware;
    std::vector<std::string> disabledModules;
    std::string saveEncryptionKey;
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
