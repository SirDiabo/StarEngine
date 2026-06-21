#pragma once
#include "raylib.h"

class Panel {
public:
    virtual ~Panel() = default;
    virtual void Initialize() {}
    virtual void Render() = 0;
    virtual void RenderHeader() {}

    const char* GetName() const { return name; }
    bool IsVisible() const { return visible; }
    void SetVisible(bool v) { visible = v; }
    bool IsDetached() const { return detached; }
    void SetDetached(bool d) { detached = d; }

    Rectangle bounds = {0, 0, 300, 400};
    Rectangle header = {0, 0, 0, 25};
    bool detached = false;
    bool dragging = false;
    Vector2 dragOffset = {0, 0};

protected:
    const char* name = "Panel";
    bool visible = true;
};