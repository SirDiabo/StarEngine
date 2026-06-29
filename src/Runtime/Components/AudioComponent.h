#pragma once
#include "SceneComponent.h"
#include <string>

class AudioComponent : public SceneComponent {
public:
    std::string soundPath;
    bool autoPlay = false;
    bool looping = false;
    float volume = 1.0f;

    void GetEditorProperties(PropertyList& outProperties) override;
    void Update(float deltaTime) override;
};