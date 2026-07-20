#pragma once
#include "SceneComponent.h"
#include <string>
#include <vector>

class InputComponent : public SceneComponent {
public:
    std::vector<std::string> mappingNames;

    const char* GetClassName() const override { return "InputComponent"; }
    void Update(float deltaTime) override;
};

void RegisterInputComponentNodes();
