#include "Widget.h"
#include "WidgetElement.h"

Widget::~Widget() {
    delete rootElement;
}

void Widget::Draw(Rectangle screenRect) {
    if (!rootElement || !rootElement->visible) return;
    rootElement->Draw(screenRect, uiScale);
}

void Widget::Tick(float deltaTime, Rectangle screenRect) {
    if (!rootElement || !rootElement->visible) return;
    rootElement->Tick(deltaTime, screenRect, uiScale);
}
