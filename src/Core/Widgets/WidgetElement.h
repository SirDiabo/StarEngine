#pragma once
#include "Runtime/Object.h"
#include "raylib.h"
#include <string>
#include <vector>
#include <functional>

class Widget;

struct WidgetMargin {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    bool IsZero() const { return left == 0.0f && top == 0.0f && right == 0.0f && bottom == 0.0f; }
};

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
    WidgetMargin margin;
    Vector2 minSize{ 0.0f, 0.0f };
    Vector2 maxSize{ 0.0f, 0.0f };
    bool visible = true;

    bool selectable = false;
    bool disabled = false;
    WidgetInteractionState interactionState = WidgetInteractionState::Default;
    std::function<void()> onActivated;
    std::function<void()> onRightActivated;
    std::function<void()> onKeyActivated;
    std::string selectionKey;
    std::string slotName;

    WidgetElement* parent = nullptr;
    Widget* owningWidget = nullptr;
    Rectangle lastResolvedRect{ 0.0f, 0.0f, 0.0f, 0.0f };

    virtual void AddChild(WidgetElement* child);
    virtual void InsertChild(WidgetElement* child, int index);
    virtual WidgetElement* DetachChild(WidgetElement* child);
    virtual void RemoveChild(WidgetElement* child);
    void RemoveChildAt(int index);
    virtual void ClearChildren();
    const std::vector<WidgetElement*>& GetChildren() const { return children; }
    virtual const std::vector<WidgetElement*>& GetAuthoredChildren() const { return children; }
    virtual void SetOwningWidgetRecursive(Widget* widget);
    static void EnsureNextElementIdAtLeast(int id);

    Rectangle ResolveRect(Rectangle parentRect, float uiScale) const;

    Rectangle InsetByMargin(Rectangle slot, float uiScale) const;
    float ConstrainWidth(float width, float uiScale) const;
    float ConstrainHeight(float height, float uiScale) const;
    Rectangle FitToSlot(Rectangle slot, float uiScale) const;

    virtual void Draw(Rectangle rect, float uiScale);
    virtual void Tick(float deltaTime, Rectangle rect, float uiScale);

    void CollectSelectables(std::vector<WidgetElement*>& out);

    virtual WidgetElement* GetNextSelectableInDirection(WidgetElement* current, int dx, int dy) const { return nullptr; }

    virtual bool RaisesPointerEvents() const { return false; }

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
