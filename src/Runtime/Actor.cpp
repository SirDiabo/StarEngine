#include "Actor.h"
#include "raymath.h"

Actor::Actor() {
    name = "Actor";
}

Actor::~Actor() {
    delete graphBehavior;
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
