#pragma once
#include "raylib.h"

class Panel {
public:
    virtual ~Panel() = default;
    virtual void Initialize() {}
    virtual void Render() = 0;

    const char* GetName() const { return name; }
    bool IsVisible() const { return visible; }
    virtual bool IsDirty() const { return false; }
    void SetVisible(bool v) { visible = v; }

    Rectangle bounds = { 0, 0, 300, 400 };

protected:
    const char* name = "Panel";
    bool visible = true;
};