#pragma once
#include "raylib.h"
#include <string>
#include <vector>

struct ProjectInfo {
    std::string name;
    std::string path;
    bool exists = false;
};

class ProjectBrowser {
public:
    enum class Mode { Open, Create };
    void Show(Mode mode = Mode::Open, bool allowCancel = false);
    void Render();
    void ResetFlags();
    void ResetCancelFlag() { wantsCancel = false; }
    Mode GetMode() const { return mode; }
    bool WantsNewProject() const { return createConfirmed; }
    bool WantsOpenProject() const { return openProject; }
    bool WantsCancel() const { return wantsCancel; }
    bool IsNameUnique(const std::string& name) const;
    std::string GetSelectedPath() const { return selectedPath; }
    std::string GetNewProjectName() const { return std::string(newProjectName); }
    bool IsVisible() const { return visible; }

private:
    bool visible = false;
    Mode mode = Mode::Open;
    bool wantsCreate = false;
    bool createConfirmed = false;
    bool openProject = false;
    bool wantsCancel = false;
    bool showCancel = false;
    std::string selectedPath;
    std::vector<ProjectInfo> projects;
    char newProjectName[256] = "";

    Texture2D logo{};
    bool logoLoaded = false;
    bool logoLoadAttempted = false;

    void ScanProjects();
    void LoadLogoIfNeeded();
};