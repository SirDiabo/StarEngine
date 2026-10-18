#pragma once
#include "WidgetElement.h"
#include <string>
#include <vector>

class Widget;
class SlotElement;

class WidgetObjectElement : public WidgetElement {
public:
    ~WidgetObjectElement() override;

    const char* GetClassName() const override { return "WidgetObjectElement"; }
    std::string GetObjectKind() const override { return "WidgetObjectElement"; }

    std::string widgetAssetPath;

    void SetWidgetAsset(const std::string& path);
    Widget* GetChildWidget() const { return childWidget; }
    std::vector<std::string> GetSlotNames() const;
    bool SetFillSlot(WidgetElement* fill, const std::string& slotName);

    void AddChild(WidgetElement* child) override;
    void InsertChild(WidgetElement* child, int index) override;
    WidgetElement* DetachChild(WidgetElement* child) override;
    void RemoveChild(WidgetElement* child) override;
    const std::vector<WidgetElement*>& GetAuthoredChildren() const override;
    void SetOwningWidgetRecursive(Widget* widget) override;

    static bool BehaviorsEnabled();
    struct BehaviorScope {
        explicit BehaviorScope(bool enabled);
        ~BehaviorScope();
    private:
        bool previous;
    };

protected:
    void OnTickSelf(float deltaTime, Rectangle rect, float uiScale) override;
    Rectangle ComputeChildRect(WidgetElement* child, Rectangle ownRect, float uiScale, int childIndex) const override;

private:
    void Build();
    void Teardown();
    bool PlaceFill(WidgetElement* fill);
    void CollectSlots(std::vector<SlotElement*>& out) const;

    Widget* childWidget = nullptr;
    std::vector<WidgetElement*> unplaced;
    mutable std::vector<WidgetElement*> authoredCache;
};
