#pragma once
#include "raylib.h"
#include "Runtime/Components/LightComponent.h"
#include <vector>

struct SceneLight {
    LightComponent::LightType type = LightComponent::Point;
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation{ 0.0f, 0.0f, 0.0f };
    Vector3 color{ 1.0f, 1.0f, 1.0f };
    float intensity = 1.0f;
    float radius = 500.0f;
    float innerConeAngle = 25.0f;
    float outerConeAngle = 35.0f;
    const void* owner = nullptr;
};

struct SceneShadowCaster {
    Model* model = nullptr;
    Matrix transform{};
    const void* owner = nullptr;
};

struct SceneSnapshot {
    std::vector<SceneLight> lights;
    std::vector<SceneShadowCaster> casters;
};

namespace SceneSnapshotSource {

inline const SceneSnapshot*& ActiveSlot() {
    static const SceneSnapshot* active = nullptr;
    return active;
}

inline const SceneSnapshot* Get() {
    return ActiveSlot();
}

class Scope {
public:
    explicit Scope(const SceneSnapshot* snapshot) : previous(ActiveSlot()) { ActiveSlot() = snapshot; }
    ~Scope() { ActiveSlot() = previous; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
private:
    const SceneSnapshot* previous;
};

}
