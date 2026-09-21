#pragma once
#include "Runtime/Object.h"
#include "raylib.h"
#include <string>
#include <vector>
#include <functional>

class Widget;

enum class WidgetInteractionState {
    Default,
    Hovered,
    Selected,
    Pressed,
    Disabled
};

class WidgetElement : public Object {
public:
    WidgetElement();
    ~WidgetElement() override;

    const char* GetClassName() const override { return "WidgetElement"; }
    std::string GetObjectKind() const override { return "WidgetElement"; }

    std::string elementId;

    Vector2 anchor{ 0.0f, 0.0f };
    Vector2 pivot{ 0.0f, 0.0f };
    Vector2 offset{ 0.0f, 0.0f };
    Vector2 size{ 100.0f, 30.0f };
    bool visible = true;

    bool selectable = false;
    bool disabled = false;
    WidgetInteractionState interactionState = WidgetInteractionState::Default;
    std::function<void()> onActivated;
    std::function<void()> onRightActivated;
    std::function<void()> onKeyActivated;
    std::string selectionKey;

    WidgetElement* parent = nullptr;
    Widget* owningWidget = nullptr;
    Rectangle lastResolvedRect{ 0.0f, 0.0f, 0.0f, 0.0f };

    void AddChild(WidgetElement* child);
    void InsertChild(WidgetElement* child, int index);
    WidgetElement* DetachChild(WidgetElement* child);
    void RemoveChild(WidgetElement* child);
    void RemoveChildAt(int index);
    void ClearChildren();
    const std::vector<WidgetElement*>& GetChildren() const { return children; }
    void SetOwningWidgetRecursive(Widget* widget);
    static void EnsureNextElementIdAtLeast(int id);

    Rectangle ResolveRect(Rectangle parentRect, float uiScale) const;

    virtual void Draw(Rectangle rect, float uiScale);
    virtual void Tick(float deltaTime, Rectangle rect, float uiScale);

    void CollectSelectables(std::vector<WidgetElement*>& out);

    virtual WidgetElement* GetNextSelectableInDirection(WidgetElement* current, int dx, int dy) const { return nullptr; }

    virtual void OnSelected();
    virtual void OnDeselected();
    virtual void OnConfirmed();

protected:
    virtual void OnDrawSelf(Rectangle rect, float uiScale) { }
    virtual void OnTickSelf(float deltaTime, Rectangle rect, float uiScale) { }
    virtual Rectangle ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const;
    void DrawChildren(Rectangle rect, float uiScale);
    void TickChildren(float deltaTime, Rectangle rect, float uiScale);

    std::vector<WidgetElement*> children;
};
