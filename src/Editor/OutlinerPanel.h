#pragma once
#include "Core/Panel.h"
#include "Runtime/StarMap.h"

class OutlinerPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

    const StarMap* starMap = nullptr;
    int* selectedActorIndex = nullptr;
};