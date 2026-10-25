#pragma once
#include "Core/Panel.h"
#include "raylib.h"

class StarscriptEditorSession;

class StarscriptScriptPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    void SetSession(StarscriptEditorSession* s) { session = s; }

private:
    StarscriptEditorSession* session = nullptr;
};

class StarscriptOutlinePanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    void SetSession(StarscriptEditorSession* s) { session = s; }

private:
    StarscriptEditorSession* session = nullptr;
    float scroll = 0.0f;
};
