#pragma once
#include "Core/Panel.h"
#include "Editor/DetailsPanel.h"
#include "raylib.h"
#include <functional>
#include <utility>

class ActorClassEditor;

class ComponentOutlinerPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    void SetEditor(ActorClassEditor* e) { editor = e; }
    void SetScreenBoundsProvider(std::function<Rectangle()> provider) { screenBounds = std::move(provider); }

private:
    ActorClassEditor* editor = nullptr;
    std::function<Rectangle()> screenBounds;
};

class ActorPreviewPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    void SetEditor(ActorClassEditor* e) { editor = e; }

private:
    ActorClassEditor* editor = nullptr;
};

class ComponentDetailsPanel : public DetailsPanel {
public:
    void SetEditor(ActorClassEditor* e);

private:
    ActorClassEditor* editor = nullptr;
    std::string lastSelectionKey;
};
