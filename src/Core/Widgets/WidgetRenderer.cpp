#include "WidgetRenderer.h"
#include "Widget.h"
#include "WidgetInput.h"
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

void TickScreenSpaceWidgets(float deltaTime, Rectangle targetRect, Vector2 localMousePos, bool nativeResolutionOnly) {
    for (const auto& actor : World::Get().GetActors()) {
        for (Component* component : actor->GetComponents()) {
            WidgetComponent* widgetComponent = dynamic_cast<WidgetComponent*>(component);
            if (!widgetComponent) continue;
            if (widgetComponent->renderSpace != WidgetRenderSpace::Screen) continue;
            if (widgetComponent->forceNativeResolution != nativeResolutionOnly) continue;
            Widget* widget = widgetComponent->GetWidget();
            if (!widget) continue;
            widget->Tick(deltaTime, targetRect);
            WidgetInput::ProcessInput(widget, widgetComponent->inputDevice, deltaTime, localMousePos);
        }
    }
}

}
