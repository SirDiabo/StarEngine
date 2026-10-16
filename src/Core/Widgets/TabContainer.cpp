#include "TabContainer.h"

void TabHeaderButtonElement::OnConfirmed() {
    if (owningTabContainer) owningTabContainer->SetActiveIndex(tabIndex);
    WidgetElement::OnConfirmed();
}

void TabHeaderButtonElement::OnDrawSelf(Rectangle rect, float uiScale) {
    bool isActive = owningTabContainer && tabIndex == owningTabContainer->activeIndex;
    switch (interactionState) {
        case WidgetInteractionState::Disabled: color = disabledColor; break;
        case WidgetInteractionState::Pressed:  color = pressedColor;  break;
        case WidgetInteractionState::Selected: color = selectedColor; break;
        case WidgetInteractionState::Hovered:  color = hoveredColor;  break;
        default:                               color = isActive ? selectedColor : defaultColor; break;
    }
    RectangleElement::OnDrawSelf(rect, uiScale);
}

TabContainer::TabContainer() {
    headerBox = new HorizontalBoxElement();
    AddChild(headerBox);
}

void TabContainer::AddTab(const std::string& label, WidgetElement* content, float headerButtonWidth) {
    TabHeaderButtonElement* button = new TabHeaderButtonElement();
    button->SetName(label);
    button->owningTabContainer = this;
    int newIndex = (int)headerBox->GetChildren().size();
    button->tabIndex = newIndex;
    button->size = { headerButtonWidth, headerHeight };
    headerBox->AddChild(button);

    content->visible = (newIndex == activeIndex);
    AddChild(content);
}

void TabContainer::SetActiveIndex(int index) {
    const auto& kids = GetChildren();
    int contentCount = (int)kids.size() - 1;
    if (index < 0 || index >= contentCount) return;
    if (activeIndex >= 0 && activeIndex < contentCount) kids[activeIndex + 1]->visible = false;
    activeIndex = index;
    kids[activeIndex + 1]->visible = true;
}

void TabContainer::Draw(Rectangle rect, float uiScale) {
    const auto& kids = GetChildren();
    if (kids.empty()) return;

    Rectangle headerRect{ rect.x, rect.y, rect.width, headerHeight * uiScale };
    kids[0]->Draw(kids[0]->FitToSlot(headerRect, uiScale), uiScale);

    int contentIndex = activeIndex + 1;
    if (contentIndex >= 1 && contentIndex < (int)kids.size()) {
        Rectangle contentRect{ rect.x, rect.y + headerRect.height, rect.width, rect.height - headerRect.height };
        kids[contentIndex]->Draw(kids[contentIndex]->FitToSlot(contentRect, uiScale), uiScale);
    }
}

void TabContainer::Tick(float deltaTime, Rectangle rect, float uiScale) {
    const auto& kids = GetChildren();
    if (kids.empty()) return;

    Rectangle headerRect{ rect.x, rect.y, rect.width, headerHeight * uiScale };
    kids[0]->Tick(deltaTime, kids[0]->FitToSlot(headerRect, uiScale), uiScale);

    int contentIndex = activeIndex + 1;
    if (contentIndex >= 1 && contentIndex < (int)kids.size()) {
        Rectangle contentRect{ rect.x, rect.y + headerRect.height, rect.width, rect.height - headerRect.height };
        kids[contentIndex]->Tick(deltaTime, kids[contentIndex]->FitToSlot(contentRect, uiScale), uiScale);
    }
}
