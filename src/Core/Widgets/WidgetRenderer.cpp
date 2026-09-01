#include "WidgetRenderer.h"
#include "Widget.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/WidgetComponent.h"

namespace WidgetRenderer {

void DrawScreenSpaceWidgets(Rectangle targetRect, bool nativeResolutionOnly) {
    for (const auto& actor : World::Get().GetActors()) {
        for (Component* component : actor->GetComponents()) {
            WidgetComponent* widgetComponent = dynamic_cast<WidgetComponent*>(component);
            if (!widgetComponent) continue;
            if (widgetComponent->renderSpace != WidgetRenderSpace::Screen) continue;
            if (widgetComponent->forceNativeResolution != nativeResolutionOnly) continue;
            Widget* widget = widgetComponent->GetWidget();
            if (widget) widget->Draw(targetRect);
        }
    }
}

void TickScreenSpaceWidgets(float deltaTime, Rectangle targetRect) {
    for (const auto& actor : World::Get().GetActors()) {
        for (Component* component : actor->GetComponents()) {
            WidgetComponent* widgetComponent = dynamic_cast<WidgetComponent*>(component);
            if (!widgetComponent) continue;
            if (widgetComponent->renderSpace != WidgetRenderSpace::Screen) continue;
            Widget* widget = widgetComponent->GetWidget();
            if (widget) widget->Tick(deltaTime, targetRect);
        }
    }
}

}
