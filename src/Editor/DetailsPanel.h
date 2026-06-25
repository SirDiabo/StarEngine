#pragma once
#include "Core/Panel.h"
#include "Runtime/StarMap.h"

class DetailsPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;

    const StarMap* starMap = nullptr;
    int* selectedActorIndex = nullptr;

private:
    char nameEditBuf[128] = "";
    char posXBuf[32] = "", posYBuf[32] = "", posZBuf[32] = "";
    int lastActorIndex = -1;
    int activeField = 0;
};