#include "Actor.h"
#include "Components/Component.h"
#include "Components/SceneComponent.h"
#include "raymath.h"
#include <algorithm>
#include <unordered_map>

static std::unordered_map<uint32_t, Actor*>& NetIdRegistry() {
    static std::unordered_map<uint32_t, Actor*> registry;
    return registry;
}

Actor::Actor() {
    name = "Actor";
    rootComponent = new SceneComponent();
    rootComponent->SetName("SceneRoot");
    components.push_back(rootComponent);
}

Actor::~Actor() {
    if (netId != 0) NetIdRegistry().erase(netId);
    delete graphBehavior;
    for (Component* c : components) delete c;
}

void Actor::SetNetId(uint32_t id) {
    if (netId != 0) NetIdRegistry().erase(netId);
    netId = id;
    if (netId != 0) NetIdRegistry()[netId] = this;
}

Actor* Actor::FindByNetId(uint32_t id) {
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
    OnTick(deltaTime);
    if (graphBehavior) graphBehavior->OnTick(this, deltaTime);

    if (useControlYaw)   rotation.y = controlRotation.y;
    if (useControlPitch) rotation.x = controlRotation.x;
    if (useControlRoll)  rotation.z = controlRotation.z;

    if (cameraComponent) {
        if (cameraComponent->useControlYaw)   cameraComponent->rotation.y = controlRotation.y - rotation.y;
        if (cameraComponent->useControlPitch) cameraComponent->rotation.x = controlRotation.x - rotation.x;
        if (cameraComponent->useControlRoll)  cameraComponent->rotation.z = controlRotation.z - rotation.z;
    }

    float moveLenSq = pendingMovementInput.x * pendingMovementInput.x
                     + pendingMovementInput.y * pendingMovementInput.y
                     + pendingMovementInput.z * pendingMovementInput.z;
    if (moveLenSq > 1.0f) pendingMovementInput = Vector3Scale(pendingMovementInput, 1.0f / sqrtf(moveLenSq));
    position = Vector3Add(position, Vector3Scale(pendingMovementInput, deltaTime));
    pendingMovementInput = { 0, 0, 0 };

    for (Component* c : components) c->Update(deltaTime);
}

void Actor::EndPlay() {
    OnEndPlay();
    if (graphBehavior) graphBehavior->OnEndPlay(this);
    hasBegunPlay = false;
}

Matrix Actor::GetWorldMatrix() const {
    Matrix scaleMatrix = MatrixScale(scale.x, scale.y, scale.z);
    Matrix rotationMatrix = MatrixRotateXYZ(Vector3{ DEG2RAD * rotation.x, DEG2RAD * rotation.y, DEG2RAD * rotation.z });
    Matrix translationMatrix = MatrixTranslate(position.x, position.y, position.z);
    return MatrixMultiply(MatrixMultiply(scaleMatrix, rotationMatrix), translationMatrix);
}

Vector3 Actor::ComputeWorldPosition(Vector3 localPos, Vector3 parentPos, Vector3 parentRot, Vector3 parentScale) {
    Vector3 scaledLocalPos = {
        localPos.x * parentScale.x,
        localPos.y * parentScale.y,
        localPos.z * parentScale.z
    };
    Matrix rotationMatrix = MatrixRotateXYZ(Vector3{ DEG2RAD * parentRot.x, DEG2RAD * parentRot.y, DEG2RAD * parentRot.z });
    Vector3 rotatedLocalPos = Vector3Transform(scaledLocalPos, rotationMatrix);
    return Vector3Add(parentPos, rotatedLocalPos);
}

Vector3 Actor::ComputeWorldRotation(Vector3 localRot, Vector3 parentRot) {
    return Vector3Add(parentRot, localRot);
}

Vector3 Actor::ComputeWorldScale(Vector3 localScale, Vector3 parentScale) {
    return Vector3Multiply(localScale, parentScale);
}

void Actor::AddComponent(Component* component) {
    components.push_back(component);
    component->parent = GetRootComponent();
    if (MeshComponent* mc = dynamic_cast<MeshComponent*>(component)) {
        meshComponent = mc;
    } else if (CollisionComponent* cc = dynamic_cast<CollisionComponent*>(component)) {
        collisionComponent = cc;
    } else if (AudioComponent* ac = dynamic_cast<AudioComponent*>(component)) {
        audioComponent = ac;
    } else if (CameraComponent* cc = dynamic_cast<CameraComponent*>(component)) {
        cameraComponent = cc;
    }
}

void Actor::RemoveComponent(Component* component) {
    auto it = std::find(components.begin(), components.end(), component);
    if (it != components.end()) {
        if (meshComponent == component) meshComponent = nullptr;
        if (collisionComponent == component) collisionComponent = nullptr;
        if (audioComponent == component) audioComponent = nullptr;
        if (cameraComponent == component) cameraComponent = nullptr;
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