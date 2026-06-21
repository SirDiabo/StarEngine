#pragma once
#include "Core/Panel.h"
#include <string>
#include <vector>

struct ProjectInfo {
    std::string name;
    std::string path;
    bool exists = false;
};

class ProjectBrowser {
public:
    void Show();
    void Render();
    bool WantsNewProject() const { return wantsCreate; }
    bool WantsOpenProject() const { return openProject; }
    std::string GetSelectedPath() const { return selectedPath; }
    std::string GetNewProjectName() const { return std::string(newProjectName); }
    void ClearCreateRequest() { wantsCreate = false; }

private:
    bool visible = false;
    bool wantsCreate = false;
    bool openProject = false;
    std::string selectedPath;
    std::vector<ProjectInfo> projects;
    char newProjectName[256] = "";

    void ScanProjects();
};