#include "GameSession.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/StarMap.h"
#include "Runtime/WorldSpawner.h"
#include "Runtime/Components/CameraComponent.h"
#include "Widgets/WidgetDebug.h"
#include "raymath.h"
#include "rlgl.h"

namespace GameSession {

void BeginSession(StarMap& map) {
    WorldSpawner::SpawnMapActors(map);
    World::Get().BeginPlay();
    WidgetDebug::SpawnTestWidget();
    WidgetDebug::SpawnContainerTestWidget();
}

void EndSession() {
    World::Get().EndPlay();
}

void UpdateActiveCamera(Camera3D& camera) {
    const auto& actors = World::Get().GetActors();
    if (actors.empty()) return;

    Actor* camActor = World::Get().GetViewTarget();
    if (!camActor) {
        for (const auto& actor : actors) {
            if (actor->GetCameraComponent()) {
                camActor = actor.get();
                World::Get().SetViewTarget(camActor);
                break;
            }
        }
    }
    if (!camActor) return;

    CameraComponent* cc = camActor->GetCameraComponent();
    if (!cc) return;

    Vector3 compPos = camActor->GetComponentWorldPosition(cc);
    Vector3 compRot = camActor->GetComponentWorldRotation(cc);
    float yr = compRot.y * DEG2RAD;
    float pr = compRot.x * DEG2RAD;
    Vector3 forward = {cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr)};
    camera.position = compPos;
    camera.target = Vector3Add(compPos, forward);
    camera.up = {0, 1, 0};
    camera.fovy = cc->fov;
    camera.projection = cc->orthographic ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
    rlSetClipPlanes((double)cc->nearClip, (double)cc->farClip);
}

}
