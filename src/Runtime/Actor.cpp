#include "Core/RotationMath.h"
#include "Actor.h"
#include "SaveGame.h"
#include "ProfilerLabels.h"
#include "Core/Profiler.h"
#include "Components/Component.h"
#include "Components/SceneComponent.h"
#include "Core/ObjectKindRegistry.h"
#include "raymath.h"
#include <algorithm>
#include <unordered_map>
#include <mutex>

void Actor::SetActorClass(const std::string& newClass) {
    actorClass = newClass;
    if (!newClass.empty()) ObjectKindRegistry::Get().RegisterKind(newClass, "Actor");
}

static std::unordered_map<uint32_t, Actor*>& NetIdRegistry() {
    static std::unordered_map<uint32_t, Actor*> registry;
    return registry;
}

static std::mutex& NetIdRegistryMutex() {
    static std::mutex m;
    return m;
}

Actor::Actor() {
    name = "Actor";
    rootComponent = new SceneComponent();
    rootComponent->SetName("SceneRoot");
    components.push_back(rootComponent);
}

Actor::~Actor() {
    if (netId != 0) {
        std::lock_guard<std::mutex> lock(NetIdRegistryMutex());
        NetIdRegistry().erase(netId);
    }
    delete graphBehavior;
    for (Component* c : components) delete c;
}

void Actor::SetNetId(uint32_t id) {
    std::lock_guard<std::mutex> lock(NetIdRegistryMutex());
    if (netId != 0) NetIdRegistry().erase(netId);
    netId = id;
    if (netId != 0) NetIdRegistry()[netId] = this;
}

Actor* Actor::FindByNetId(uint32_t id) {
    std::lock_guard<std::mutex> lock(NetIdRegistryMutex());
    auto it = NetIdRegistry().find(id);
    return it != NetIdRegistry().end() ? it->second : nullptr;
}

void Actor::GetProperties(PropertyList& outProperties) {
    Object::GetProperties(outProperties);
    outProperties.AddVector3("Position", &position.x);
    outProperties.AddVector3("Rotation", &rotation.x);
    outProperties.AddVector3("Scale", &scale.x);
    outProperties.AddBool("Show Mouse Cursor", &showMouseCursor);
    outProperties.AddBool("Use Control Yaw", &useControlYaw);
    outProperties.AddBool("Use Control Pitch", &useControlPitch);
    outProperties.AddBool("Use Control Roll", &useControlRoll);
}

void Actor::SaveState(const std::string& saveFile) {
    SaveGame::SaveActor(this, saveFile);
}

void Actor::LoadState(const std::string& saveFile) {
    SaveGame::LoadActor(this, saveFile);
}

void Actor::SetGraphBehavior(GraphBehavior* behavior) {
    delete graphBehavior;
    graphBehavior = behavior;
}

void Actor::BeginPlay() {
    controlRotation = rotation;
    hasBegunPlay = true;
    OnBeginPlay();
    if (graphBehavior) graphBehavior->OnBeginPlay(this);
}

void Actor::Tick(float deltaTime) {
    {
        PROFILE_SCOPE("Native OnTick");
        OnTick(deltaTime);
    }
    if (graphBehavior) graphBehavior->OnTick(this, deltaTime);

    if (useControlYaw)   rotation.y = controlRotation.y;
    if (useControlPitch) rotation.x = controlRotation.x;
    if (useControlRoll)  rotation.z = controlRotation.z;

    if (CameraComponent* cam = GetCameraComponent()) {
        if (cam->useControlYaw)   cam->rotation.y = controlRotation.y - rotation.y;
        if (cam->useControlPitch) cam->rotation.x = controlRotation.x - rotation.x;
        if (cam->useControlRoll)  cam->rotation.z = controlRotation.z - rotation.z;
    }

    float moveLenSq = pendingMovementInput.x * pendingMovementInput.x
                     + pendingMovementInput.y * pendingMovementInput.y
                     + pendingMovementInput.z * pendingMovementInput.z;
    if (moveLenSq > 1.0f) pendingMovementInput = Vector3Scale(pendingMovementInput, 1.0f / sqrtf(moveLenSq));
    position = Vector3Add(position, Vector3Scale(pendingMovementInput * MovementSpeed, deltaTime));
    pendingMovementInput = { 0, 0, 0 };

    PROFILE_SCOPE("Components");
    for (Component* c : components) {
        PROFILE_SCOPE_DYN(ProfilerLabels::ForComponent(*c));
        c->Update(deltaTime);
    }
}

void Actor::EndPlay() {
    OnEndPlay();
    if (graphBehavior) graphBehavior->OnEndPlay(this);
    hasBegunPlay = false;
}

Matrix Actor::GetWorldMatrix() const {
    Matrix scaleMatrix = MatrixScale(scale.x, scale.y, scale.z);
    Matrix rotationMatrix = ComposeRotationDegrees(rotation);
    Matrix translationMatrix = MatrixTranslate(position.x, position.y, position.z);
    return MatrixMultiply(MatrixMultiply(scaleMatrix, rotationMatrix), translationMatrix);
}

Vector3 Actor::ComputeWorldPosition(Vector3 localPos, Vector3 parentPos, Vector3 parentRot, Vector3 parentScale) {
    Vector3 scaledLocalPos = {
        localPos.x * parentScale.x,
        localPos.y * parentScale.y,
        localPos.z * parentScale.z
    };
    Matrix rotationMatrix = ComposeRotationDegrees(parentRot);
    Vector3 rotatedLocalPos = Vector3Transform(scaledLocalPos, rotationMatrix);
    return Vector3Add(parentPos, rotatedLocalPos);
}

Vector3 Actor::ComputeWorldRotation(Vector3 localRot, Vector3 parentRot) {
    return Vector3Add(parentRot, localRot);
}

Vector3 Actor::ComputeWorldScale(Vector3 localScale, Vector3 parentScale) {
    return Vector3Multiply(localScale, parentScale);
}

void Actor::GetComponentWorldTransform(Component* comp, Vector3& outPos, Vector3& outRot, Vector3& outScale) const {
    Component* chain[64];
    int count = 0;
    Component* c = comp;
    while (c && count < 64) {
        chain[count++] = c;
        c = c->parent;
    }

    outPos = position;
    outRot = rotation;
    outScale = scale;
    for (int i = count - 1; i >= 0; i--) {
        Component* cur = chain[i];
        outPos = ComputeWorldPosition(cur->position, outPos, outRot, outScale);
        outRot = ComputeWorldRotation(cur->rotation, outRot);
        outScale = ComputeWorldScale(cur->scale, outScale);
    }
}

Vector3 Actor::GetComponentWorldScale(Component* comp) const {
    Vector3 pos, rot, scl;
    GetComponentWorldTransform(comp, pos, rot, scl);
    return scl;
}

void Actor::SetComponentWorldPosition(Component* comp, Vector3 worldPos) {
    if (!comp) return;
    Vector3 pPos, pRot, pScale;
    GetComponentWorldTransform(comp->parent, pPos, pRot, pScale);
    Vector3 rotatedLocalPos = Vector3Subtract(worldPos, pPos);
    Matrix rotationMatrix = MatrixRotateXYZ(Vector3{ DEG2RAD * pRot.x, DEG2RAD * pRot.y, DEG2RAD * pRot.z });
    Matrix invRot = MatrixInvert(rotationMatrix);
    Vector3 scaledLocalPos = Vector3Transform(rotatedLocalPos, invRot);
    comp->position = {
        pScale.x != 0.0f ? scaledLocalPos.x / pScale.x : 0.0f,
        pScale.y != 0.0f ? scaledLocalPos.y / pScale.y : 0.0f,
        pScale.z != 0.0f ? scaledLocalPos.z / pScale.z : 0.0f
    };
}

void Actor::SetComponentWorldRotation(Component* comp, Vector3 worldRot) {
    if (!comp) return;
    Vector3 pPos, pRot, pScale;
    GetComponentWorldTransform(comp->parent, pPos, pRot, pScale);
    comp->rotation = Vector3Subtract(worldRot, pRot);
}

void Actor::SetComponentWorldScale(Component* comp, Vector3 worldScale) {
    if (!comp) return;
    Vector3 pPos, pRot, pScale;
    GetComponentWorldTransform(comp->parent, pPos, pRot, pScale);
    comp->scale = {
        pScale.x != 0.0f ? worldScale.x / pScale.x : 0.0f,
        pScale.y != 0.0f ? worldScale.y / pScale.y : 0.0f,
        pScale.z != 0.0f ? worldScale.z / pScale.z : 0.0f
    };
}

Vector3 Actor::GetComponentWorldPosition(Component* comp) const {
    Vector3 pos, rot, scl;
    GetComponentWorldTransform(comp, pos, rot, scl);
    return pos;
}

Vector3 Actor::GetComponentWorldRotation(Component* comp) const {
    Vector3 pos, rot, scl;
    GetComponentWorldTransform(comp, pos, rot, scl);
    return rot;
}

void Actor::AddComponent(Component* component) {
    components.push_back(component);
    component->parent = GetRootComponent();
    component->owningActor = this;
}

void Actor::RemoveComponent(Component* component) {
    auto it = std::find(components.begin(), components.end(), component);
    if (it != components.end()) {
        if (rootComponent == component) rootComponent = nullptr;
        components.erase(it);
        delete component;
    }
}

Component* Actor::GetRootComponent() {
    if (!rootComponent) {
        rootComponent = new SceneComponent();
        rootComponent->SetName("SceneRoot");
        components.push_back(rootComponent);
    }
    return rootComponent;
}

Component* Actor::FindComponentByName(const std::string& name) const {
    for (Component* c : components) {
        if (c->GetName() == name) return c;
    }
    return nullptr;
}

void Actor::AddControlYawInput(float value) {
    controlRotation.y += value;
}

void Actor::AddControlPitchInput(float value) {
    controlRotation.x += value;
    if (controlRotation.x > 89.0f) controlRotation.x = 89.0f;
    if (controlRotation.x < -89.0f) controlRotation.x = -89.0f;
}

void Actor::AddControlRollInput(float value) {
    controlRotation.z += value;
}

void Actor::AddMovementInput(Vector3 worldDirection, float scaleValue) {
    float len = Vector3Length(worldDirection);
    if (len > 0.0001f) worldDirection = Vector3Scale(worldDirection, 1.0f / len);
    pendingMovementInput = Vector3Add(pendingMovementInput, Vector3Scale(worldDirection, scaleValue));
}
