#pragma once
#include "SceneComponent.h"
#include <string>

class BillboardComponent : public SceneComponent {
public:
    std::string texturePath;
    float width = 50.0f;
    float height = 50.0f;
    bool hiddenInGame = true; // hidden during PIE and standalone play, visible in the editor

    const char* GetClassName() const override { return "BillboardComponent"; }
    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;
};
