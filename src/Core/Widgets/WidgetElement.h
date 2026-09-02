#pragma once
#include "Runtime/Object.h"
#include "raylib.h"
#include <string>
#include <vector>

enum class WidgetInteractionState {
    Default,
    Hovered,
    Selected,
    Pressed,
    Disabled
};

class WidgetElement : public Object {
public:
    ~WidgetElement() override;

    const char* GetClassName() const override { return "WidgetElement"; }
    std::string GetObjectKind() const override { return "WidgetElement"; }

    Vector2 anchor{ 0.0f, 0.0f };
    Vector2 pivot{ 0.0f, 0.0f };
    Vector2 offset{ 0.0f, 0.0f };
    Vector2 size{ 100.0f, 30.0f };
    bool visible = true;

    bool selectable = false;
    bool disabled = false;
    WidgetInteractionState interactionState = WidgetInteractionState::Default;

    WidgetElement* parent = nullptr;
    Rectangle lastResolvedRect{ 0.0f, 0.0f, 0.0f, 0.0f };

    void AddChild(WidgetElement* child);
    void RemoveChild(WidgetElement* child);
    void RemoveChildAt(int index);
    void ClearChildren();
    const std::vector<WidgetElement*>& GetChildren() const { return children; }

    Rectangle ResolveRect(Rectangle parentRect, float uiScale) const;

    virtual void Draw(Rectangle rect, float uiScale);
    virtual void Tick(float deltaTime, Rectangle rect, float uiScale);

    void CollectSelectables(std::vector<WidgetElement*>& out);

    virtual void OnSelected() { }
    virtual void OnDeselected() { }
    virtual void OnConfirmed() { }

protected:
    virtual void OnDrawSelf(Rectangle rect, float uiScale) { }
    virtual void OnTickSelf(float deltaTime, Rectangle rect, float uiScale) { }
    virtual Rectangle ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const;
    void DrawChildren(Rectangle rect, float uiScale);
    void TickChildren(float deltaTime, Rectangle rect, float uiScale);

    std::vector<WidgetElement*> children;
};
