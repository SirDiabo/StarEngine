#pragma once

class Panel {
public:
    virtual ~Panel() = default;
    virtual void Initialize() = 0;
    virtual void Render() = 0;

    const char* GetName() const { return name; }
    bool IsVisible() const { return visible; }
    void SetVisible(bool v) { visible = v; }

protected:
    const char* name = "Panel";
    bool visible = true;
};