#include "Core/RotationMath.h"
#include "WidgetRenderer.h"
#include "Widget.h"
#include "WidgetInput.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/WidgetComponent.h"
#include "Runtime/ProfilerLabels.h"
#include "Core/ProfilerRender.h"
#include "raymath.h"

namespace WidgetRenderer {

void DrawScreenSpaceWidgets(Rectangle targetRect, bool nativeResolutionOnly, float bufferScale) {
    for (const auto& actor : World::Get().GetActors()) {
        for (Component* component : actor->GetComponents()) {
            WidgetComponent* widgetComponent = dynamic_cast<WidgetComponent*>(component);
            if (!widgetComponent) continue;
            if (widgetComponent->renderSpace != WidgetRenderSpace::Screen) continue;
            if (widgetComponent->forceNativeResolution != nativeResolutionOnly) continue;
            Widget* widget = widgetComponent->GetWidget();
            if (!widget) continue;
            PROFILE_SCOPE_DYN("Draw: " + ProfilerLabels::ForActor(*actor) + " / " + ProfilerLabels::ForComponent(*widgetComponent));
            widget->Draw(targetRect, bufferScale);
        }
    }
}

void TickScreenSpaceWidgets(float deltaTime, Rectangle targetRect, Vector2 localMousePos, bool nativeResolutionOnly, float bufferScale) {
    for (const auto& actor : World::Get().GetActors()) {
        for (Component* component : actor->GetComponents()) {
            WidgetComponent* widgetComponent = dynamic_cast<WidgetComponent*>(component);
            if (!widgetComponent) continue;
            if (widgetComponent->renderSpace != WidgetRenderSpace::Screen) continue;
            if (widgetComponent->forceNativeResolution != nativeResolutionOnly) continue;
            Widget* widget = widgetComponent->GetWidget();
            if (!widget) continue;
            PROFILE_SCOPE_DYN("Tick: " + ProfilerLabels::ForActor(*actor) + " / " + ProfilerLabels::ForComponent(*widgetComponent));
            widget->Tick(deltaTime, targetRect, bufferScale);
            WidgetInput::ProcessInput(widget, widgetComponent->inputDevice, deltaTime, localMousePos, bufferScale);
        }
    }
}

static constexpr float kWorldPlaneBaseRotationDeg = 90.0f;

static Model& GetWidgetPlaneModel() {
    static Model planeModel = LoadModelFromMesh(GenMeshPlane(1.0f, 1.0f, 1, 1));
    return planeModel;
}

static const int kWorldTextureSize = 512;

void PrepareWorldSpaceWidgets() {
    for (const auto& actor : World::Get().GetActors()) {
        for (Component* component : actor->GetComponents()) {
            WidgetComponent* widgetComponent = dynamic_cast<WidgetComponent*>(component);
            if (!widgetComponent) continue;
            if (widgetComponent->renderSpace != WidgetRenderSpace::World &&
                widgetComponent->renderSpace != WidgetRenderSpace::Billboard) continue;

            Widget* widget = widgetComponent->GetWidget();
            if (!widget) continue;

            RenderTexture2D& rt = widgetComponent->GetOrCreateWorldTexture(kWorldTextureSize, kWorldTextureSize);

            PROFILE_RENDER_SCOPE_DYN("Render To Texture: " + ProfilerLabels::ForActor(*actor) + " / " + ProfilerLabels::ForComponent(*widgetComponent));
            BeginTextureMode(rt);
            ClearBackground(BLANK);
            widget->Tick(GetFrameTime(), {0, 0, (float)kWorldTextureSize, (float)kWorldTextureSize}, 1.0f);
            widget->Draw({0, 0, (float)kWorldTextureSize, (float)kWorldTextureSize}, 1.0f);
            EndTextureMode();
        }
    }
}

void DrawWorldSpaceWidgets(Camera3D camera) {
    for (const auto& actor : World::Get().GetActors()) {
        for (Component* component : actor->GetComponents()) {
            WidgetComponent* widgetComponent = dynamic_cast<WidgetComponent*>(component);
            if (!widgetComponent) continue;
            if (widgetComponent->renderSpace != WidgetRenderSpace::World &&
                widgetComponent->renderSpace != WidgetRenderSpace::Billboard) continue;

            Widget* widget = widgetComponent->GetWidget();
            if (!widget) continue;

            RenderTexture2D& rt = widgetComponent->GetOrCreateWorldTexture(kWorldTextureSize, kWorldTextureSize);

            Vector3 pos, rot, scl;
            actor->GetComponentWorldTransform(component, pos, rot, scl);

            Rectangle source = { 0, 0, (float)kWorldTextureSize, -(float)kWorldTextureSize };
            Vector2 size = { scl.x, scl.y };

            if (widgetComponent->renderSpace == WidgetRenderSpace::Billboard) {
                DrawBillboardRec(camera, rt.texture, source, pos, size, WHITE);
            } else {
                Model& plane = GetWidgetPlaneModel();
                plane.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = rt.texture;

                Matrix baseRotation = MatrixRotateX(DEG2RAD * kWorldPlaneBaseRotationDeg);
                Matrix scaleMatrix = MatrixScale(scl.x, scl.y, 1.0f);
                Matrix rotationMatrix = ComposeRotationDegrees(rot);

                Matrix savedTransform = plane.transform;
                plane.transform = MatrixMultiply(MatrixMultiply(scaleMatrix, baseRotation), rotationMatrix);
                DrawModel(plane, pos, 1.0f, WHITE);
                plane.transform = savedTransform;
            }
        }
    }
}

}
