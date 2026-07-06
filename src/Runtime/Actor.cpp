#include "Actor.h"
#include "Components/Component.h"
#include "Components/SceneComponent.h"
#include "raymath.h"
#include <algorithm>

Actor::Actor() {
    name = "Actor";
    rootComponent = new SceneComponent();
    rootComponent->SetName("SceneRoot");
    components.push_back(rootComponent);
}

Actor::~Actor() {
    delete graphBehavior;
    for (Component* c : components) delete c;
}

void Actor::GetProperties(PropertyList& outProperties) {
    Object::GetProperties(outProperties);
    outProperties.AddVector3("Position", &position.x);
    outProperties.AddVector3("Rotation", &rotation.x);
    outProperties.AddVector3("Scale", &scale.x);
}

void Actor::SetGraphBehavior(GraphBehavior* behavior) {
    delete graphBehavior;
    graphBehavior = behavior;
}

void Actor::BeginPlay() {
    hasBegunPlay = true;
    OnBeginPlay();
    if (graphBehavior) graphBehavior->OnBeginPlay(this);
}

void Actor::Tick(float deltaTime) {
    OnTick(deltaTime);
    if (graphBehavior) graphBehavior->OnTick(this, deltaTime);
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