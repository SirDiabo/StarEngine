#pragma once
#include "WidgetElement.h"
#include "HorizontalBoxElement.h"
#include "ButtonElement.h"
#include <string>

class TabContainer;

class TabHeaderButtonElement : public ButtonElement {
public:
    const char* GetClassName() const override { return "TabHeaderButtonElement"; }
    std::string GetObjectKind() const override { return "TabHeaderButtonElement"; }

    TabContainer* owningTabContainer = nullptr;
    int tabIndex = 0;

    void OnConfirmed() override;

protected:
    void OnDrawSelf(Rectangle rect, float uiScale) override;
};

class TabContainer : public WidgetElement {
public:
    TabContainer();

    const char* GetClassName() const override { return "TabContainer"; }
    std::string GetObjectKind() const override { return "TabContainer"; }

    int activeIndex = 0;
    float headerHeight = 32.0f;

    void AddTab(const std::string& label, WidgetElement* content, float headerButtonWidth = 120.0f);
    void SetActiveIndex(int index);

    void Draw(Rectangle rect, float uiScale) override;
    void Tick(float deltaTime, Rectangle rect, float uiScale) override;

private:
    HorizontalBoxElement* headerBox = nullptr;
};
