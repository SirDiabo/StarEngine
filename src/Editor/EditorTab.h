#pragma once
#include <string>

class MenuBar;

class EditorTab {
public:
    virtual ~EditorTab() = default;

    virtual void Initialize() {}
    virtual void Render(float x, float y, float width, float height) = 0;

    virtual void BuildMenus(MenuBar* menuBar) = 0;

    virtual void OnActivated() {}
    virtual void OnDeactivated() {}

    virtual bool IsOpen() const { return true; }

    virtual std::string GetAssetPath() const { return ""; }

    virtual void SaveLayout(const std::string& filePath) const {}
    virtual void LoadLayout(const std::string& filePath) {}
    virtual void ResetLayout() {}
    virtual void TogglePanelVisible(const std::string& name) {}
    virtual void RenderOverlays() {}

    const std::string& GetTitle() const { return title; }
    bool IsPermanent() const { return permanent; }

protected:
    std::string title = "Editor";
    bool permanent = false;
};